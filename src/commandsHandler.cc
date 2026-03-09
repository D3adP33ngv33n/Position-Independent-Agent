#include "terminal.h"
#include "primitives.h"
#include "memory.h"
#include "file.h"
#include "directory_iterator.h"
#include "string.h"
#include "math.h"
#include "logger.h"
#include "sha2.h"
#include "uuid.h"

VOID Handle_GetUUIDCommand([[maybe_unused]] PCHAR command, [[maybe_unused]] USIZE commandLength, PPCHAR response, PUSIZE responseLength)
{
    auto result = UUID::FromString(AGENT_UUID);

    if (!result.IsOk())
    {
        LOG_ERROR("Failed to parse agent UUID string");
        *response = new CHAR[*responseLength];
        *(PUINT32)*response = 1; // Example error code for UUID parsing failure
        return;
    }

    UUID uuid = result.Value();

    *responseLength += sizeof(UUID);
    *response = new CHAR[*responseLength];
    *(PUINT32)*response = 0; // Example error code
    Memory::Copy(*response + sizeof(UINT32), &uuid, sizeof(UUID));
}

VOID Handle_GetDirectoryContentCommand([[maybe_unused]] PCHAR command, [[maybe_unused]] USIZE commandLength, PPCHAR response, PUSIZE responseLength)
{
    PWCHAR directoryPath = (PWCHAR)(command);
    LOG_INFO("Getting directory content for path: %ws", directoryPath);

    auto result = DirectoryIterator::Create(directoryPath);

    if (!result.IsOk())
    {
        LOG_ERROR("Invalid directory path: %ws", directoryPath);
        *response = new CHAR[*responseLength];
        *(PUINT32)*response = 1; // Example error code for invalid path
        return;
    }
    else
    {
        DirectoryIterator &iter = result.Value();

        // Skip first 8 bytes where we will write the number of entries as a UINT64
        *responseLength += 8; // Start with 8 bytes for entry count
        UINT64 entryCount = 0;

        while (iter.Next())
        {
            const DirectoryEntry &entry = iter.Get();

            if (StringUtils::Equals((PWCHAR)entry.Name, (const WCHAR *)L"."_embed) ||
                StringUtils::Equals((PWCHAR)entry.Name, (const WCHAR *)L".."_embed))
                continue; // Skip current and parent directory entries

            UINT64 entrySize = sizeof(DirectoryEntry);
            *responseLength += entrySize;
            entryCount++;
        }

        *response = new CHAR[*responseLength];

        // Write entry count at the beginning of the response
        Memory::Copy(*response + sizeof(UINT32), &entryCount, sizeof(UINT64));
        UINT64 offset = sizeof(UINT32) + sizeof(UINT64);         // Start after entry count
        auto result2 = DirectoryIterator::Create(directoryPath); // Reset iterator
        if (!result2.IsOk())
        {
            LOG_ERROR("Failed to reinitialize directory iterator for path: %ws", directoryPath);
            *response = new CHAR[*responseLength];
            *(PUINT32)*response = 1; // Example error code for reinitialization failure
            return;
        }
        DirectoryIterator &iter2 = result2.Value();

        while (iter2.Next())
        {
            const DirectoryEntry &entry = iter2.Get();

            if (StringUtils::Equals((PWCHAR)entry.Name, (const WCHAR *)L"."_embed) ||
                StringUtils::Equals((PWCHAR)entry.Name, (const WCHAR *)L".."_embed))
                continue; // Skip current and parent directory entries

            Memory::Copy(*response + offset, &entry, sizeof(DirectoryEntry));
            offset += sizeof(DirectoryEntry);
        }
        return;
    }
}

VOID Handle_GetFileContentCommand([[maybe_unused]] PCHAR command, [[maybe_unused]] USIZE commandLength, PPCHAR response, PUSIZE responseLength)
{
    UINT64 readCount = *(PUINT64)(command);               // Read count starts at the beginning of command
    UINT64 offset = *(PUINT64)(command + sizeof(UINT64)); // Offset starts after read count
    PWCHAR filePath = (PWCHAR)(command + sizeof(UINT64) + sizeof(UINT64));
    LOG_INFO("Getting file content for path: %ws", filePath);
    auto openResult = File::Open(filePath, File::ModeRead);

    if (!openResult)
    {
        LOG_ERROR("Failed to open file: %ws", filePath);
        *response = new CHAR[*responseLength];
        *(PUINT32)*response = 1; // Example error code for invalid file
        return;
    }
    else
    {
        File &file = openResult.Value();
        *responseLength += (USIZE)readCount + sizeof(UINT64); // Add space for read count in response
        *response = new CHAR[*responseLength];
        USIZE responseOffset = sizeof(UINT32) + sizeof(UINT64); // Start after error code and read count
        (void)file.SetOffset((USIZE)offset);
        auto readResult = file.Read(Span<UINT8>((UINT8 *)(*response + responseOffset), (USIZE)readCount));
        UINT32 bytesRead = readResult ? readResult.Value() : 0;
        // write bytes read after error code
        *(PUINT32)*response = 0;                            // Write bytes read as status code
        *(PUINT64)(*response + sizeof(UINT32)) = bytesRead; // Write bytes read after error code
        return;
    }
}

VOID Handle_GetFileChunkHashCommand([[maybe_unused]] PCHAR command, [[maybe_unused]] USIZE commandLength, PPCHAR response, PUSIZE responseLength)
{
    UINT64 chunkSize = *(PUINT64)(command);                                // Chunk size starts after command type byte
    UINT64 offset = *(PUINT64)(command + sizeof(UINT64));                  // Offset starts after command type and chunk size
    PWCHAR filePath = (PWCHAR)(command + sizeof(UINT64) + sizeof(UINT64)); // File path starts after command type, chunk size and offset
    LOG_INFO("Getting file chunk hash for path: %ws", filePath);
    auto openResult = File::Open(filePath, File::ModeRead);

    if (!openResult)
    {
        LOG_ERROR("Failed to open file: %ws", filePath);
        *response = new CHAR[*responseLength]; // Only return error code
        *(PUINT32)*response = 1;               // Example error code for invalid file
        return;
    }
    else
    {
        File &file = openResult.Value();
        UINT64 sizeOfChunk = Math::Min((UINT64)chunkSize, (UINT64)0xffff);
        PUINT8 chunkData = new UINT8[sizeOfChunk];

        SHA256 sha256;
        USIZE totalRead = 0;

        while (totalRead < chunkSize)
        {
            UINT64 bytesToRead = Math::Min(sizeOfChunk, chunkSize - totalRead);
            (void)file.SetOffset((USIZE)(offset + totalRead));
            auto readResult = file.Read(Span<UINT8>(chunkData, (USIZE)bytesToRead));
            UINT32 bytesRead = readResult ? readResult.Value() : 0;
            if (bytesRead == 0)
                break; // EOF

            sha256.Update(Span<const UINT8>(chunkData, bytesRead));
            totalRead += bytesRead;
        }
        delete[] chunkData;

        *responseLength += SHA256_DIGEST_SIZE; // Add space for status code and hash digest
        *response = new CHAR[*responseLength];

        UINT8 digest[SHA256_DIGEST_SIZE];
        sha256.Final(Span<UINT8, SHA256_DIGEST_SIZE>(digest));
        Memory::Copy(*response + sizeof(UINT32), digest, SHA256_DIGEST_SIZE);

        // set status code to 0 for success
        *(PUINT32)*response = 0;
        LOG_INFO("File chunk hash computed successfully for %llu bytes read", totalRead);
        return;
    }
}
