# Filesystem Operations

Platform-independent filesystem abstraction for file I/O, directory management, path manipulation, and directory enumeration — all implemented over raw kernel syscalls and firmware protocols.

## File Map

```
fs/
├── file.h                             # RAII file handle with read/write/seek
├── directory.h                        # Static create/delete directory methods
├── directory_entry.h                  # Directory entry metadata structure
├── directory_iterator.h               # Iterator for directory enumeration
├── path.h                             # Path manipulation utilities
├── offset_origin.h                    # Seek origin enum (Start/Current/End)
│
├── posix/file.cc                      # POSIX file I/O (Linux, macOS, FreeBSD, Solaris, Android, iOS)
├── posix/directory.cc                 # POSIX directory create/delete
├── posix/directory_iterator.cc        # POSIX directory enumeration (getdents64/getdirentries)
│
├── windows/file.cc                    # Windows file I/O (NTDLL ZwCreateFile/ZwReadFile/ZwWriteFile)
├── windows/directory.cc               # Windows directory create/delete
├── windows/directory_iterator.cc      # Windows directory enumeration (ZwQueryDirectoryFile + drive map)
│
├── uefi/file.cc                       # UEFI file I/O (EFI_FILE_PROTOCOL)
├── uefi/directory.cc                  # UEFI directory create/delete
├── uefi/directory_iterator.cc         # UEFI directory enumeration
└── uefi/uefi_fs_helpers.cc            # UEFI filesystem protocol helpers
```

## File Class

RAII file handle — move-only, stack-only, non-copyable. Destructor auto-closes the handle.

### Open Flags

| Flag | Purpose |
|---|---|
| `ModeRead` | Open for reading |
| `ModeWrite` | Open for writing |
| `ModeAppend` | Append to existing content |
| `ModeCreate` | Create if doesn't exist |
| `ModeTruncate` | Truncate existing content |
| `ModeBinary` | Binary mode (no CR/LF translation) |

### Methods

| Method | Signature | Purpose |
|---|---|---|
| `Open` | `static Open(path, flags) → Result<File, Error>` | Open or create a file |
| `Delete` | `static Delete(path) → Result<void, Error>` | Delete a file |
| `Exists` | `static Exists(path) → Result<void, Error>` | Check if file exists |
| `Read` | `Read(Span<UINT8>) → Result<UINT32, Error>` | Read bytes into buffer |
| `Write` | `Write(Span<const UINT8>) → Result<UINT32, Error>` | Write bytes from buffer |
| `GetOffset` | `GetOffset() → Result<USIZE, Error>` | Get current file position |
| `SetOffset` | `SetOffset(USIZE) → Result<void, Error>` | Set absolute file position |
| `MoveOffset` | `MoveOffset(SSIZE, OffsetOrigin) → Result<void, Error>` | Seek relative to Start/Current/End |
| `GetSize` | `GetSize() → USIZE` | Get file size (cached at open) |
| `Close` | `Close() → void` | Close file handle |

### Platform Implementations

| Platform | Open | Read/Write | Seek |
|---|---|---|---|
| **POSIX** | `open()`/`openat()` syscall | `read()`/`write()` | `lseek()` |
| **Windows** | `NTDLL::ZwCreateFile` | `ZwReadFile`/`ZwWriteFile` | `ZwSetInformationFile` (FilePositionInformation) |
| **UEFI** | `EFI_FILE_PROTOCOL→Open` | `→Read`/`→Write` | `→SetPosition` |

#### Architecture-Specific Notes

- **RISC-V 32 (Linux):** Uses `sys_llseek` (5 arguments) since 32-bit `lseek` can't handle 64-bit offsets
- **FreeBSD i386:** Requires manual stack manipulation for 64-bit offset parameters in `mmap`

## Directory Class

Static methods for directory management.

| Method | Signature |
|---|---|
| `Create` | `static Create(path) → Result<void, Error>` |
| `Delete` | `static Delete(path) → Result<void, Error>` |

## DirectoryEntry Structure

Packed structure (no padding) for directory entry metadata:

```c
struct DirectoryEntry {
    WCHAR Name[256];             // null-terminated entry name
    UINT64 CreationTime;         // filetime format
    UINT64 LastModifiedTime;     // filetime format
    UINT64 Size;                 // file size in bytes
    UINT32 Type;                 // drive type / d_type
    BOOL IsDirectory;
    BOOL IsDrive;                // Windows only: drive letter entry
    BOOL IsHidden;
    BOOL IsSystem;
    BOOL IsReadOnly;
};
```

## DirectoryIterator Class

RAII iterator for directory enumeration — move-only, stack-only.

### Methods

| Method | Signature | Purpose |
|---|---|---|
| `Create` | `static Create(path) → Result<DirectoryIterator, Error>` | Open directory for enumeration |
| `Next` | `Next() → Result<void, Error>` | Advance to next entry |
| `Get` | `Get() → const DirectoryEntry&` | Get current entry |
| `IsValid` | `IsValid() → BOOL` | More entries available? |
| `Close` | `Close() → void` | Close directory handle |

### Platform Enumeration Details

| Platform | Syscall | Directory Entry Type | Notes |
|---|---|---|---|
| **Linux/Android** | `getdents64` | `LinuxDirent64` | Has `d_type` field |
| **macOS/iOS** | `getdirentries64` | `BsdDirent64` | Uses `basep` pointer |
| **FreeBSD** | `getdirentries` | `FreeBsdDirent` | Has `d_type` + `d_namlen` |
| **Solaris** | `getdents` | `SolarisDirent64` | **No** `d_type` — needs `fstatat` per entry |
| **Windows** | `ZwQueryDirectoryFile` | `FILE_BOTH_DIR_INFORMATION` | Full metadata in one call |
| **UEFI** | `EFI_FILE_PROTOCOL→Read` | `EFI_FILE_INFO` | Full metadata in one call |

#### Windows Drive Enumeration

When the path is empty (root), Windows iterates drive letters:
1. `ZwQueryInformationProcess(ProcessDeviceMap)` → bitmask of active drives
2. Iterate bits 0-25 (A: through Z:)
3. `ZwQueryVolumeInformationFile` for drive type (fixed, remote, removable, etc.)

## Path Class

All methods are `static constexpr` — no heap allocations, no side effects.

| Method | Purpose |
|---|---|
| `Combine<TChar>(path1, path2, out)` | Join paths with platform separator |
| `GetFileName<TChar>(fullPath, out)` | Extract filename from path |
| `GetExtension<TChar>(fileName, out)` | Extract file extension |
| `GetDirectoryName<TChar>(fullPath, out)` | Extract directory portion |
| `IsPathRooted<TChar>(path)` | Check if path is absolute |
| `NormalizePath(path, out)` | Normalize separators to platform convention |

### Path Separator

| Platform | Separator | Example |
|---|---|---|
| Windows / UEFI | `\\` | `C:\\Users\\file.txt` |
| POSIX (all) | `/` | `/home/user/file.txt` |

## Platform Support

| Platform | File | Directory | Iterator | Path |
|---|---|---|---|---|
| Windows | Full | Full | Full + drive enum | `\\` separator |
| Linux | Full | Full | Full (getdents64) | `/` separator |
| Android | Full | Full | Full (getdents64) | `/` separator |
| macOS | Full | Full | Full (getdirentries64) | `/` separator |
| iOS | Full | Full | Full (getdirentries64) | `/` separator |
| FreeBSD | Full | Full | Full (getdirentries) | `/` separator |
| Solaris | Full | Full | Full (getdents + fstatat) | `/` separator |
| UEFI | Full | Full | Full (EFI_FILE_PROTOCOL) | `\\` separator |
