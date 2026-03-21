# Memory Allocation

Platform-independent virtual memory allocation. Provides the `Allocator` interface and global `operator new`/`operator delete` overloads so all heap allocations route through platform-specific virtual memory syscalls.

## File Map

```
memory/
├── allocator.h            # Interface: AllocateMemory(), ReleaseMemory()
├── allocator.cc           # Global operator new/delete overloads
├── posix/memory.cc        # POSIX: mmap/munmap
├── windows/memory.cc      # Windows: ZwAllocateVirtualMemory/ZwFreeVirtualMemory
└── uefi/memory.cc         # UEFI: AllocatePool/FreePool
```

## Allocator Class

Static class — no instantiation.

| Method | Signature | Purpose |
|---|---|---|
| `AllocateMemory` | `static AllocateMemory(USIZE size) → PVOID` | Allocate memory |
| `ReleaseMemory` | `static ReleaseMemory(PVOID ptr, USIZE size) → void` | Free memory |

## Global Operator Overloads

All C++ heap allocations are intercepted and routed through `Allocator`:

```c++
void* operator new(size_t size)         { return Allocator::AllocateMemory(size); }
void* operator new[](size_t size)       { return Allocator::AllocateMemory(size); }
void  operator delete(void* p) noexcept { Allocator::ReleaseMemory(p, 0); }
void  operator delete(void* p, size_t s) noexcept { Allocator::ReleaseMemory(p, s); }
// ... and array variants
```

## Platform Implementations

### POSIX (mmap/munmap)

```
AllocateMemory(size):
  1. actual_size = size + sizeof(USIZE)    ← header for size tracking
  2. mmap(NULL, actual_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)
  3. Store actual_size in header
  4. Return pointer past header

ReleaseMemory(ptr, size):
  1. Read stored size from header (ptr - sizeof(USIZE))
  2. munmap(header_ptr, stored_size)
```

**Why the size header?** `munmap` requires the size of the mapping. Since C++ `operator delete(void*)` doesn't provide the size, the allocator prepends a `USIZE` header to store it.

#### Architecture-Specific Notes

| Architecture | Syscall | Notes |
|---|---|---|
| **x86_64** | `SYS_MMAP` (9) | Standard 6-arg mmap |
| **i386** | `SYS_MMAP2` | Page-shifted offset (offset/4096) |
| **AArch64** | `SYS_MMAP` (222) | Standard 6-arg |
| **ARMv7-A** | `SYS_MMAP2` | Page-shifted offset |
| **RISC-V 64/32** | `SYS_MMAP` | Standard 6-arg |
| **MIPS64** | `SYS_MMAP` | Standard 6-arg |
| **FreeBSD i386** | `SYS_MMAP` (477) | Requires inline asm for 64-bit offset on stack |

### Windows (ZwAllocateVirtualMemory)

```
AllocateMemory(size):
  ZwAllocateVirtualMemory(NtCurrentProcess(), &base, 0, &size,
                          MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)

ReleaseMemory(ptr, size):
  ZwFreeVirtualMemory(NtCurrentProcess(), &ptr, &zero, MEM_RELEASE)
```

No size header needed — `MEM_RELEASE` frees the entire region.

### UEFI (AllocatePool/FreePool)

```
AllocateMemory(size):
  BootServices->AllocatePool(EfiLoaderData, size, &buffer)

ReleaseMemory(ptr, size):
  BootServices->FreePool(ptr)
```

No size header needed — UEFI pool allocator tracks sizes internally.

## Design Notes

- All allocations are page-aligned on POSIX (minimum 4096 bytes per mmap)
- No CRT `malloc`/`free` — completely standalone
- Allocations come directly from the OS virtual memory manager
- Thread safety depends on the underlying OS syscall guarantees
