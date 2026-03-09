#include "primitives.h"

enum CommandType : UINT8
{
    GetUUID = 0,
    GetDirectoryContent = 1,
    GetFileContent = 2,
    GetFileChunkHash = 3
};

#define AGENT_UUID (PCCHAR) "12345678-9abc-def0-1234-56789abcdef0"_embed

// Handle functions for each command type
VOID Handle_GetUUIDCommand(PCHAR command, USIZE commandLength, PPCHAR response, PUSIZE responseLength);
VOID Handle_GetDirectoryContentCommand(PCHAR command, USIZE commandLength, PPCHAR response, PUSIZE responseLength);
VOID Handle_GetFileContentCommand(PCHAR command, USIZE commandLength, PPCHAR response, PUSIZE responseLength);
VOID Handle_GetFileChunkHashCommand(PCHAR command, USIZE commandLength, PPCHAR response, PUSIZE responseLength);