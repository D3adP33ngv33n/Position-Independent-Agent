# Platform Layer

Cross-platform abstraction layer providing OS-independent interfaces for I/O, networking, memory, display, and process management. Every implementation uses **direct syscalls and firmware protocols** — no CRT, no SDK, no libc dependencies.

## Architecture

```
┌──────────────────────────────────────────────────────────────────────┐
│                        platform.h  (aggregate header)                │
├────────┬──────────┬─────────┬─────────┬──────────┬──────────────────┤
│Console │Filesystem│ Memory  │ Screen  │ Socket   │     System       │
│        │          │         │         │          │ DateTime Random  │
│ Write  │ File     │Allocator│GetDevice│ Create   │ MachineID Env    │
│ Logger │ Directory│ new/del │ Capture │ Open     │ Pipe Process     │
│        │ Iterator │         │         │ Read     │ Pty ShellProcess │
│        │ Path     │         │         │ Write    │ SystemInfo       │
├────────┴──────────┴─────────┴─────────┴──────────┴──────────────────┤
│                         Kernel Interface                             │
│  ┌─────────┬─────────┬───────┬─────┬─────────┬────────┬──────────┐  │
│  │ Windows │  Linux  │FreeBSD│macOS│ Solaris  │  UEFI  │ Android  │  │
│  │ PEB/PE  │ syscall │ int80 │ XNU │ int 0x91 │Protocol│ (Linux)  │  │
│  │ Indirect│ svc/int │ svc   │svc80│ svc/ecall│ Tables │          │  │
│  │ Syscall │ ecall   │ ecall │     │          │        │  iOS     │  │
│  └─────────┴─────────┴───────┴─────┴─────────┴────────┴──────────┘  │
└──────────────────────────────────────────────────────────────────────┘
```

## Module Documentation

| # | Module | README | Purpose |
|---|---|---|---|
| 1 | [Console](console/) | [console/README.md](console/README.md) | Text output, UTF-16/8 conversion, ANSI-colored logging |
| 2 | [Filesystem](fs/) | [fs/README.md](fs/README.md) | RAII file I/O, directories, path manipulation, directory enumeration |
| 3 | [Memory](memory/) | [memory/README.md](memory/README.md) | Virtual memory allocation, global operator new/delete |
| 4 | [Screen](screen/) | [screen/README.md](screen/README.md) | Display enumeration, framebuffer capture |
| 5 | [Socket](socket/) | [socket/README.md](socket/README.md) | TCP stream sockets (IPv4/IPv6), Windows AFD, UEFI TCP |
| 6 | [System](system/) | [system/README.md](system/README.md) | DateTime, Random, MachineID, Environment, Pipe, Process, PTY, Shell |

## Kernel Interface Documentation

| # | Platform | README | Architectures |
|---|---|---|---|
| 1 | [Windows](kernel/windows/) | [kernel/windows/README.md](kernel/windows/README.md) | x86_64, i386, ARM64, ARM32 |
| 2 | [Linux](kernel/linux/) | [kernel/linux/README.md](kernel/linux/README.md) | x86_64, i386, AArch64, ARMv7-A, RV64, RV32, MIPS64 |
| 3 | [FreeBSD](kernel/freebsd/) | [kernel/freebsd/README.md](kernel/freebsd/README.md) | x86_64, i386, AArch64, RISC-V 64 |
| 4 | [macOS](kernel/macos/) | [kernel/macos/README.md](kernel/macos/README.md) | x86_64, AArch64 |
| 5 | [iOS](kernel/ios/) | [kernel/ios/README.md](kernel/ios/README.md) | AArch64 |
| 6 | [Android](kernel/android/) | [kernel/android/README.md](kernel/android/README.md) | (inherits Linux) |
| 7 | [Solaris](kernel/solaris/) | [kernel/solaris/README.md](kernel/solaris/README.md) | x86_64, i386, AArch64 |
| 8 | [UEFI](kernel/uefi/) | [kernel/uefi/README.md](kernel/uefi/README.md) | x86_64, AArch64 |
| 9 | [NetBSD](kernel/netbsd/) | [kernel/netbsd/README.md](kernel/netbsd/README.md) | Not implemented |

## Platform Support Matrix

### Module × Platform

| Module | Windows | Linux | Android | macOS | iOS | FreeBSD | Solaris | UEFI |
|---|---|---|---|---|---|---|---|---|
| **Console** | Full | Full | Full | Full | Full | Full | Full | Full |
| **Filesystem** | Full | Full | Full | Full | Full | Full | Full | Full |
| **Memory** | Full | Full | Full | Full | Full | Full | Full | Full |
| **Screen** | Full | Full | Full | Full | Stub | Full | Full | Full |
| **Socket** | Full (AFD) | Full | Full | Full | Full | Full | Full | Full (TCP proto) |
| **DateTime** | Full | Full | Full | Full | Full | Full | Full | Full |
| **Random** | Full | Full | Full | Full | Full | Full | Full | Full |
| **MachineID** | SMBIOS | /etc/machine-id | boot_id | /etc/machine-id | — | /etc/machine-id | — | Stub |
| **Environment** | PEB | environ | environ | environ | environ | environ | environ | Stub |
| **Pipe** | Full | Full | Full | Full | Full | Full | Full | — |
| **Process** | Full | Full | Full | Full | Full | Full | Full | — |
| **PTY** | — | Full | Full | Full | Full | Full | Full | — |
| **ShellProcess** | cmd.exe | /bin/sh | /bin/sh | /bin/sh | /bin/sh | /bin/sh | /bin/sh | — |

### Architecture × Platform

| | i386 | x86_64 | ARMv7-A | AArch64 | RV32 | RV64 | MIPS64 |
|---|---|---|---|---|---|---|---|
| **Windows** | Yes | Yes | Yes | Yes | — | — | — |
| **Linux** | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| **Android** | Yes | Yes | Yes | Yes | — | — | — |
| **macOS** | — | Yes | — | Yes | — | — | — |
| **iOS** | — | — | — | Yes | — | — | — |
| **FreeBSD** | Yes | Yes | — | Yes | — | Yes | — |
| **Solaris** | Yes | Yes | — | Yes | — | — | — |
| **UEFI** | — | Yes | — | Yes | — | — | — |

## Design Principles

1. **Zero Dependencies** — No CRT, no SDK headers, no libc. All platform access is via direct syscalls or firmware protocols.
2. **Position-Independent** — No static data, no import tables, no relocations. All code runs from `.text`-only memory.
3. **RAII Everywhere** — File, Socket, Pipe, Process, Pty, ShellProcess are all move-only, stack-only RAII types with automatic cleanup.
4. **Uniform Error Handling** — All fallible operations return `Result<T, Error>`. Platform-specific status codes (NTSTATUS, errno, EFI_STATUS) are converted to a unified `Error` type.
5. **Compile-Time Platform Selection** — Platform-specific implementations are selected via `#if defined(PLATFORM_*)` at compile time. No runtime dispatch overhead.
6. **Stack-Only Enforcement** — Core types prohibit heap allocation to ensure predictability in memory-constrained environments.
