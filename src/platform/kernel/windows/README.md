[< Back to Platform](../../README.md) | [< Back to Project Root](../../../../README.md)

# Windows NT Kernel Interface

Position-independent Windows NT kernel layer providing direct system call dispatch, PEB-based module resolution, PE export parsing, and Win32 API wrappers — all without any dependency on the Windows SDK, CRT, or static import tables.

## Architecture Support

| Architecture | Register for PEB | Syscall Mechanism | Calling Convention |
|---|---|---|---|
| **x86_64** | `GS:[0x60]` | Indirect `syscall` gadget (`0F 05 C3`) in ntdll | `R10/RDX/R8/R9` + stack shadow space |
| **i386** | `FS:[0x30]` | `call [edx]` or `call edx` via ntdll stub | All args pushed on stack, `EAX` = SSN |
| **ARM64** | `X18 + 0x60` | `BLR` to ntdll `SVC #N; RET` stub | `X0-X7` + stack |
| **ARM32** | `R9 + 0x30` | `BLX` to ntdll `SVC #1` stub (Thumb-2) | `R0-R3` + stack |

## Documentation

Detailed documentation for each subsystem:

| # | Document | Description | Source Files |
|---|---|---|---|
| 1 | [PEB Walking & Module Resolution](docs/PEB_WALKING.md) | TEB/PEB access, loader data structures, `InMemoryOrderModuleList` traversal, DJB2 hash matching, fast/slow path module resolution | `peb.h`, `peb.cc` |
| 2 | [PE Format Parsing](docs/PE_PARSING.md) | DOS/NT headers, export directory, name-to-ordinal-to-RVA resolution, forwarded export handling, PE32 vs PE32+ differences | `pe.h`, `pe.cc` |
| 3 | [Indirect Syscall Dispatch](docs/INDIRECT_SYSCALLS.md) | SSN resolution, ntdll stub scanning, gadget discovery, per-architecture inline assembly dispatch (0-14 args), `ResolveSyscall` macro | `system.h`, `system.cc`, `system.*.h` |
| 4 | [NT Native API Wrappers](docs/NTDLL_WRAPPERS.md) | 23 `Zw*` syscall wrappers, 5 `Rtl*` runtime library functions, dual-path dispatch pattern, NTSTATUS error handling | `ntdll.h`, `ntdll.cc` |
| 5 | [Win32 API Wrappers](docs/WIN32_WRAPPERS.md) | Kernel32 (process/pipe), User32 (display enumeration), GDI32 (screen capture), dynamic resolution pattern | `kernel32.*`, `user32.*`, `gdi32.*` |

## File Map

```
windows/
├── peb.h / peb.cc             # PEB walking and module resolution
├── pe.h / pe.cc               # PE format parsing and export resolution
├── system.h / system.cc       # SSN resolution and syscall dispatch core
├── system.x86_64.h            # x86_64 indirect syscall inline assembly
├── system.i386.h              # i386 indirect syscall inline assembly
├── system.aarch64.h           # ARM64 indirect syscall inline assembly
├── system.armv7a.h            # ARM32 indirect syscall inline assembly
├── ntdll.h / ntdll.cc         # NT Native API (Zw*/Rtl*) wrappers
├── kernel32.h / kernel32.cc   # Win32 kernel32.dll wrappers
├── user32.h / user32.cc       # Win32 user32.dll wrappers
├── gdi32.h / gdi32.cc         # Win32 gdi32.dll wrappers
├── windows_types.h            # NT fundamental types and constants
├── platform_result.h          # NTSTATUS → Result<T, Error> conversion
└── docs/
    ├── PEB_WALKING.md         # PEB structures, list traversal, TEB registers
    ├── PE_PARSING.md          # DOS/NT headers, export resolution, forwarded exports
    ├── INDIRECT_SYSCALLS.md   # SSN resolution, gadget scanning, per-arch dispatch
    ├── NTDLL_WRAPPERS.md      # All Zw*/Rtl* functions, dual-path dispatch pattern
    └── WIN32_WRAPPERS.md      # Kernel32, User32, GDI32 wrapper APIs
```

## How It All Connects

```
                    ┌─────────────────────────────────┐
                    │        Win32 Wrappers            │
                    │  Kernel32 · User32 · GDI32       │
                    └───────────────┬──────────────────┘
                                    │ ResolveExportAddress()
                    ┌───────────────▼──────────────────┐
                    │      NT Native API (NTDLL)       │
                    │   ZwCreateFile · ZwReadFile ...   │
                    └──────┬────────────────┬──────────┘
                           │                │
              ResolveSyscall()      CALL_FUNCTION()
              (indirect path)       (direct fallback)
                           │                │
                    ┌──────▼───────┐  ┌─────▼──────────┐
                    │   System     │  │ PEB Export      │
                    │   ::Call()   │  │ Resolution      │
                    │  (inline     │  │ (ResolveExport  │
                    │   assembly)  │  │  Address)       │
                    └──────┬───────┘  └─────┬──────────┘
                           │                │
                    ┌──────▼───────┐  ┌─────▼──────────┐
                    │  Syscall     │  │  PEB Walking    │
                    │  Gadget in   │  │  GetModuleHandle│
                    │  ntdll.dll   │  │  FromPEB()      │
                    └──────┬───────┘  └─────┬──────────┘
                           │                │
                    ┌──────▼───────┐  ┌─────▼──────────┐
                    │  PE Export   │  │  PE Export      │
                    │  Parsing     │  │  Parsing        │
                    │  (system.cc) │  │  (pe.cc)        │
                    └──────────────┘  └────────────────┘
```

## Quick Reference

### Wrapped Syscalls (23 functions)

| Category | Functions |
|---|---|
| **File I/O** | `ZwCreateFile`, `ZwOpenFile`, `ZwReadFile`, `ZwWriteFile`, `ZwClose`, `ZwDeleteFile` |
| **File Metadata** | `ZwQueryInformationFile`, `ZwSetInformationFile`, `ZwQueryAttributesFile`, `ZwQueryDirectoryFile`, `ZwQueryVolumeInformationFile` |
| **Memory** | `ZwAllocateVirtualMemory`, `ZwFreeVirtualMemory`, `ZwProtectVirtualMemory` |
| **Process** | `ZwTerminateProcess`, `ZwCreateUserProcess`, `ZwQueryInformationProcess` |
| **Sync** | `ZwCreateEvent`, `ZwWaitForSingleObject` |
| **Device/Pipe** | `ZwDeviceIoControlFile`, `ZwCreateNamedPipeFile`, `ZwSetInformationObject` |
| **System** | `ZwQuerySystemInformation` |

### Win32 Wrappers (15 functions)

| DLL | Functions |
|---|---|
| **kernel32** | `CreateProcessW`, `SetHandleInformation`, `CreatePipe`, `PeekNamedPipe` |
| **user32** | `EnumDisplayDevicesW`, `EnumDisplaySettingsW`, `GetDC`, `ReleaseDC` |
| **gdi32** | `CreateCompatibleDC`, `CreateCompatibleBitmap`, `SelectObject`, `BitBlt`, `GetDIBits`, `DeleteDC`, `DeleteObject` |

### Windows Types (`windows_types.h`)

| Category | Types / Constants |
|---|---|
| **NT Objects** | `UNICODE_STRING`, `OBJECT_ATTRIBUTES`, `IO_STATUS_BLOCK`, `LARGE_INTEGER` |
| **File** | `FILE_OPEN`, `FILE_CREATE`, `FILE_SYNCHRONOUS_IO_NONALERT`, `FILE_ATTRIBUTE_*` |
| **Access** | `GENERIC_READ`, `GENERIC_WRITE`, `SYNCHRONIZE`, `DELETE` |
| **Memory** | `MEM_COMMIT`, `MEM_RESERVE`, `MEM_RELEASE`, `PAGE_READWRITE` |
| **Firmware** | `SYSTEM_FIRMWARE_TABLE_INFORMATION`, `RAW_SMBIOS_DATA`, `SMBIOS_HEADER` |
| **Helpers** | `InitializeObjectAttributes`, `INVALID_HANDLE_VALUE` |

### NTSTATUS Error Handling (`platform_result.h`)

`result::FromNTSTATUS<T>(status)` — converts `NTSTATUS` to `Result<T, Error>` using NT_SUCCESS semantics (`status >= 0`). Failed statuses are wrapped in `Error::Windows(status)`.

## Design Principles

1. **Zero Dependencies** — No Windows SDK, no CRT, no static import tables. All types are custom-defined.
2. **Position-Independent** — All module and function resolution happens at runtime via PEB walking and DJB2 hashing. No IAT entries exist.
3. **No Data Sections** — String literals and constants are compiled into the `.text` section via a custom LLVM pass, ensuring the entire binary operates from read-execute memory.
4. **Indirect Syscalls** — System calls go through gadgets found within ntdll.dll rather than calling ntdll exports directly, making the calls originate from ntdll's address space.
5. **Multi-Architecture** — Identical API surface across x86_64, i386, ARM64, and ARM32 with architecture-specific inline assembly isolated in per-arch headers.
