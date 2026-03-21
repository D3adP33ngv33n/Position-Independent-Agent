# Console I/O

Platform-independent text output and structured logging, implemented directly over kernel syscalls without any CRT dependency.

## File Map

```
console/
├── console.h              # Interface: Write(), WriteFormatted()
├── console.cc             # UTF-16 → UTF-8 conversion (shared by POSIX platforms)
├── posix/console.cc       # Linux, Android, macOS, iOS, FreeBSD, Solaris
├── windows/console.cc     # Windows (NTDLL ZwWriteFile + WriteConsoleW)
├── uefi/console.cc        # UEFI (EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL)
└── logger.h               # Structured logging with ANSI colors and timestamps
```

## Console Class

Static class — no instantiation, all methods are static.

### Output Methods

| Method | Purpose |
|---|---|
| `Write(Span<const CHAR> text)` | Write narrow string (UTF-8 on POSIX, ANSI on Windows) |
| `Write(Span<const WCHAR> text)` | Write wide string (native UTF-16 on Windows, converted to UTF-8 on POSIX) |
| `Write<TChar>(const TChar* text)` | Write null-terminated string |
| `WriteFormatted<TChar, Args...>(format, args...)` | Printf-style formatted output (variadic templates, type-safe) |

### Platform Implementations

| Platform | Narrow Output | Wide Output |
|---|---|---|
| **POSIX** | `write(STDOUT_FILENO)` syscall | UTF-16 → UTF-8 conversion, then `write()` |
| **Windows** | `ZwWriteFile` to `StandardOutput` handle | `WriteConsoleW` (native Unicode) |
| **UEFI** | Convert to CHAR16, then `OutputString` | Direct `EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL→OutputString` |

### Wide-to-Narrow Conversion

The shared `console.cc` provides UTF-16 → UTF-8 conversion for POSIX platforms. Since Windows handles wide strings natively via `WriteConsoleW`, only POSIX and UEFI need conversion paths.

## Logger Class

Static structured logging with ANSI color-coded output and timestamps.

### Log Levels

| Method | Color | Prefix |
|---|---|---|
| `Logger::Info(format, args...)` | Green (`\033[0;32m`) | `[INF]` |
| `Logger::Warning(format, args...)` | Yellow (`\033[0;33m`) | `[WRN]` |
| `Logger::Error(format, args...)` | Red (`\033[0;31m`) | `[ERR]` |
| `Logger::Debug(format, args...)` | Cyan (`\033[0;36m`) | `[DBG]` |

### Output Format

```
\033[0;32m[INF] [14:30:45] Connection established to 192.168.1.1:443\033[0m
```

- Timestamps via `DateTime::Now().ToTimeOnlyString()` (`HH:MM:SS`)
- Uses `Console::WriteFormatted` internally
- Zero-overhead when `ENABLE_LOGGING` is disabled at compile time

## Platform Support

| Platform | Status |
|---|---|
| Windows | Full (native UTF-16) |
| Linux / Android | Full (UTF-8 via syscall) |
| macOS / iOS | Full (UTF-8 via syscall) |
| FreeBSD | Full (UTF-8 via syscall) |
| Solaris | Full (UTF-8 via syscall) |
| UEFI | Full (CHAR16 via protocol) |
