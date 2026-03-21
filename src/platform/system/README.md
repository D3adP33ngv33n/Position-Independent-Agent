# System Utilities

Platform-independent system services: date/time, random number generation, machine identification, environment variables, process management, pseudo-terminals, pipes, and interactive shell sessions.

## File Map

```
system/
├── date_time.h                         # DateTime class with formatting
├── random.h                            # Hardware-seeded PRNG
├── machine_id.h                        # Machine-unique UUID retrieval
├── environment.h                       # Environment variable access
├── pipe.h                              # Anonymous inter-process pipe
├── process.h                           # Child process creation and management
├── pty.h                               # Pseudo-terminal (master/slave pair)
├── shell_process.h                     # Interactive shell with I/O redirection
├── system_info.h                       # System info structure (UUID, hostname, arch)
│
├── posix/date_time.cc                  # clock_gettime (Linux, macOS, FreeBSD, Solaris)
├── posix/environment.cc                # Process environ pointer
├── posix/pipe.cc                       # pipe()/pipe2() syscall
├── posix/process.cc                    # fork() + execve()
├── posix/pty.cc                        # /dev/ptmx + unlockpt
├── posix/shell_process.cc              # /bin/sh via PTY
├── posix/system_info.cc               # /etc/machine-id, hostname
│
├── windows/date_time.cc               # NtQuerySystemTime, QueryPerformanceCounter
├── windows/environment.cc             # PEB environment block
├── windows/pipe.cc                    # CreatePipe (kernel32)
├── windows/process.cc                 # CreateProcessW
├── windows/shell_process.cc           # cmd.exe via 3 pipes
├── windows/system_info.cc            # SMBIOS UUID, COMPUTERNAME
│
├── uefi/date_time.cc                  # EFI_RUNTIME_SERVICES→GetTime
├── uefi/environment.cc               # Stub (not available)
└── uefi/system_info.cc              # Defaults
```

## DateTime Class

### Fields

```c
struct DateTime {
    UINT64 Years;           UINT32 Months;        UINT32 Days;
    UINT32 Hours;           UINT32 Minutes;       UINT32 Seconds;
    UINT64 Milliseconds;    UINT64 Microseconds;  UINT64 Nanoseconds;
};
```

### Methods

| Method | Signature | Purpose |
|---|---|---|
| `Now` | `static Now() → DateTime` | Get current wall-clock time |
| `GetMonotonicNanoseconds` | `static → UINT64` | Monotonic counter (for intervals) |
| `ToTimeOnlyString<TChar>` | `→ TimeOnlyString<TChar>` | Format as `"HH:MM:SS"` |
| `ToDateOnlyString<TChar>` | `→ DateOnlyString<TChar>` | Format as `"YYYY-MM-DD"` |
| `ToDateTimeString<TChar>` | `→ DateTimeString<TChar>` | Format as `"YYYY-MM-DD HH:MM:SS"` |
| `IsLeapYear` | `static constexpr → BOOL` | Leap year check |
| `GetDaysInMonth` | `static constexpr → UINT32` | Days in a given month |

### Platform Implementations

| Platform | Wall Clock | Monotonic Clock |
|---|---|---|
| **Windows** | `NtQuerySystemTime` (FILETIME since 1601-01-01) | `QueryPerformanceCounter` |
| **POSIX** | `clock_gettime(CLOCK_REALTIME)` | `clock_gettime(CLOCK_MONOTONIC)` |
| **UEFI** | `EFI_RUNTIME_SERVICES→GetTime` | `WaitForEvent` timer |

## Random Class

Hardware-seeded pseudo-random number generator.

### Methods

| Method | Signature | Purpose |
|---|---|---|
| `Get` | `Get() → INT32` | Random integer (0 to `Prng::Max`) |
| `GetArray` | `GetArray(Span<UINT8>)` | Fill buffer with random bytes |
| `GetChar<TChar>` | `→ TChar` | Random lowercase letter (a-z) |
| `GetString<TChar>` | `GetString(Span<TChar>) → UINT32` | Random lowercase string |
| `RandomUUID` | `→ UUID` | Version 4 UUID (RFC 9562) |

### Hardware Seed Sources

| Architecture | Seed Instruction | Register |
|---|---|---|
| **x86 / x86_64** | `RDTSC` | Time Stamp Counter |
| **AArch64** | `MRS CNTVCT_EL0` | System counter |
| **RISC-V 64** | `RDTIME` | CSR time register |
| **32-bit platforms** | `DateTime::GetMonotonicNanoseconds()` | Software fallback |

## Machine ID

### `GetMachineUUID() → Result<UUID, Error>`

| Platform | Source | Persistence |
|---|---|---|
| **Windows** | SMBIOS Type 1 UUID via `NtQuerySystemInformation` | Survives OS reinstall (hardware) |
| **Linux** | `/etc/machine-id` (systemd) | Persists across reboots |
| **Android** | `/etc/machine-id` or `/proc/sys/kernel/random/boot_id` | Per-boot or per-install |
| **FreeBSD** | `/etc/machine-id` | Persists across reboots |
| **UEFI** | — | Not available (returns default) |

## Environment Class

### `GetVariable(name, buffer) → USIZE`

| Platform | Source | Notes |
|---|---|---|
| **Windows** | `PEB→ProcessParameters→Environment` | Wide string environment block |
| **POSIX** | `extern char** environ` | Standard process environ pointer |
| **UEFI** | — | Always returns 0 (no environment) |

## Pipe Class

RAII anonymous pipe — move-only, stack-only.

| Method | Purpose |
|---|---|
| `Create() → Result<Pipe, Error>` | Create read/write pipe pair |
| `Read(Span<UINT8>) → Result<USIZE, Error>` | Read from pipe |
| `Write(Span<const UINT8>) → Result<USIZE, Error>` | Write to pipe |
| `CloseRead() / CloseWrite()` | Close individual ends |
| `ReadEnd() / WriteEnd() → SSIZE` | Get raw fd/handle |

| Platform | Implementation |
|---|---|
| **POSIX** | `pipe()` or `pipe2()` syscall |
| **Windows** | `Kernel32::CreatePipe` |
| **UEFI** | Not supported |

## Process Class

RAII child process — move-only, stack-only.

| Method | Purpose |
|---|---|
| `Create(path, args[], stdin, stdout, stderr) → Result<Process, Error>` | Spawn child |
| `Wait() → Result<SSIZE, Error>` | Wait for exit, return exit code |
| `Terminate() → Result<void, Error>` | Force kill (SIGKILL / TerminateProcess) |
| `IsRunning() → BOOL` | Check if still alive |
| `Id() → SSIZE` | PID or process ID |

| Platform | Implementation |
|---|---|
| **POSIX** | `fork()` + `execve()`, `wait4()`, `kill(SIGKILL)` |
| **Windows** | `Kernel32::CreateProcessW`, `ZwWaitForSingleObject`, `ZwTerminateProcess` |
| **UEFI** | Not supported |

## Pty Class

RAII pseudo-terminal — move-only, stack-only. POSIX only.

| Method | Purpose |
|---|---|
| `Create() → Result<Pty, Error>` | Open master/slave PTY pair |
| `Read(Span<UINT8>) → Result<USIZE, Error>` | Read from master |
| `Write(Span<const UINT8>) → Result<USIZE, Error>` | Write to master |
| `Poll(timeoutMs) → SSIZE` | Check for data (>0 available, 0 timeout, <0 error) |
| `SlaveFd() / MasterFd() → SSIZE` | Get raw file descriptors |
| `CloseSlave()` | Close slave end after fork |

### PTY Creation Flow (POSIX)

```
1. open("/dev/ptmx", O_RDWR | O_NOCTTY) → master fd
2. ioctl(master, TIOCSPTLCK, &unlock)   → unlock slave
3. ioctl(master, TIOCGPTN, &pty_num)    → get slave number
4. Construct slave path: "/dev/pts/{num}"
5. open(slave_path, O_RDWR) → slave fd
```

**FreeBSD variant:** Uses `TIOCPTYGRANT`, `TIOCPTUNLK`, `TIOCGPTN` ioctls.

| Platform | Supported |
|---|---|
| Linux / Android | Yes (`/dev/ptmx`) |
| macOS / iOS | Yes (`/dev/ptmx`) |
| FreeBSD | Yes (`/dev/ptmx` with FreeBSD-specific ioctls) |
| Solaris | Yes (`/dev/ptmx`) |
| Windows / UEFI | Not supported |

## ShellProcess Class

Interactive shell with I/O redirection — combines Process/Pipe/Pty.

| Method | Purpose |
|---|---|
| `Create() → Result<ShellProcess, Error>` | Start interactive shell |
| `Write(data, length) → Result<USIZE, Error>` | Send input to shell |
| `Read(buffer, capacity) → Result<USIZE, Error>` | Read shell output |
| `ReadError(buffer, capacity) → Result<USIZE, Error>` | Read stderr (Windows only) |
| `Poll(timeoutMs) → SSIZE` | Check for output data |
| `EndOfLineChar() → CHAR` | `'>'` on Windows, `'$'` on POSIX |

### Platform Implementations

| Platform | Shell | I/O Method |
|---|---|---|
| **POSIX** | `/bin/sh` | PTY (single fd for stdin+stdout) |
| **Windows** | `cmd.exe` | 3 pipes (stdin, stdout, stderr) |
| **UEFI** | — | Not supported |

## SystemInfo Structure

```c
struct SystemInfo {
    UUID MachineUUID;          // Hardware/OS-level unique identifier
    CHAR Hostname[256];        // Null-terminated hostname
    CHAR Architecture[32];     // "x86_64", "aarch64", etc. (compile-time)
    CHAR Platform[32];         // "windows", "linux", etc. (compile-time)
};
```

### `GetSystemInfo(SystemInfo* info)`

| Platform | UUID Source | Hostname Source |
|---|---|---|
| **Windows** | SMBIOS | `COMPUTERNAME` env var |
| **POSIX** | `/etc/machine-id` | `HOSTNAME` env var or `/etc/hostname` |
| **UEFI** | Default | Default |

## Platform Support Matrix

| Component | Windows | POSIX | UEFI |
|---|---|---|---|
| DateTime | Full | Full | Full |
| Random | Full | Full | Full |
| MachineID | Full (SMBIOS) | Full (/etc/machine-id) | Stub |
| Environment | Full (PEB) | Full (environ) | Stub |
| Pipe | Full | Full | Not supported |
| Process | Full | Full | Not supported |
| Pty | Not supported | Full | Not supported |
| ShellProcess | Full (cmd.exe) | Full (/bin/sh) | Not supported |
| SystemInfo | Full | Full | Defaults |
