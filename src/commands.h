#pragma once

#include "primitives.h"

enum CommandType : UINT8
{
    GetUUID = 0,
    GetDirectoryContent = 1,
    GetFileContent = 2,
    GetFileChunkHash = 3,
    CommandTypeCount
};

enum StatusCode : UINT32
{
    Success = 0,
    Error = 1,
    UnknownCommand = 2
};

#define AGENT_UUID (PCCHAR) "12345678-9abc-def0-1234-56789abcdef0"_embed

using CommandHandler = VOID (*)(PCHAR command, USIZE commandLength, PPCHAR response, PUSIZE responseLength);

VOID Handle_GetUUIDCommand(PCHAR command, USIZE commandLength, PPCHAR response, PUSIZE responseLength);
VOID Handle_GetDirectoryContentCommand(PCHAR command, USIZE commandLength, PPCHAR response, PUSIZE responseLength);
VOID Handle_GetFileContentCommand(PCHAR command, USIZE commandLength, PPCHAR response, PUSIZE responseLength);
VOID Handle_GetFileChunkHashCommand(PCHAR command, USIZE commandLength, PPCHAR response, PUSIZE responseLength);
