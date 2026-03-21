# TCP Socket Networking

Platform-independent TCP stream socket implementation supporting IPv4 and IPv6 across all platforms. Each platform uses its native networking interface — POSIX sockets, Windows AFD driver, or UEFI TCP protocols.

## File Map

```
socket/
├── socket.h               # Interface: Socket, SockAddr, SockAddr6, SocketAddressHelper
├── posix/socket.cc        # POSIX (Linux, Android, macOS, iOS, FreeBSD, Solaris)
├── windows/socket.cc      # Windows (AFD driver via NTDLL ioctls)
└── uefi/socket.cc         # UEFI (EFI_TCP4/TCP6_PROTOCOL)
```

## Socket Address Structures

### SockAddr (IPv4)

```c
struct SockAddr {
#if defined(BSD)                // macOS, iOS, FreeBSD
    UINT8 SinLen;               // Structure length (BSD requirement)
    UINT8 SinFamily;            // AF_INET
#else                           // Linux, Windows, Solaris, UEFI
    UINT16 SinFamily;           // AF_INET
#endif
    UINT16 SinPort;             // Port in network byte order
    UINT32 SinAddr;             // IPv4 address in network byte order
    CHAR SinZero[8];            // Padding to 16 bytes
};
```

### SockAddr6 (IPv6)

```c
struct SockAddr6 {
#if defined(BSD)
    UINT8 Sin6Len;
    UINT8 Sin6Family;           // AF_INET6
#else
    UINT16 Sin6Family;          // AF_INET6
#endif
    UINT16 Sin6Port;            // Port in network byte order
    UINT32 Sin6Flowinfo;        // Flow label
    UINT8 Sin6Addr[16];         // 128-bit IPv6 address
    UINT32 Sin6ScopeId;         // Scope ID
};
```

### Platform-Specific Constants

| Constant | Linux | macOS/iOS | FreeBSD | Solaris | Windows | UEFI |
|---|---|---|---|---|---|---|
| `AF_INET` | 2 | 2 | 2 | 2 | 2 | 2 |
| `AF_INET6` | 10 | 30 | 28 | 26 | 23 | 23 |
| `SOCK_STREAM` | 1 | 1 | 1 | 2 | 1 | — |
| `SOCK_DGRAM` | 2 | 2 | 2 | 1 | 2 | — |

**Note:** Solaris and MIPS Linux swap `SOCK_STREAM` and `SOCK_DGRAM` values (inherited from SVR4/IRIX).

## SocketAddressHelper

Static utility class for address preparation:

| Method | Purpose |
|---|---|
| `PrepareAddress(ip, port, buffer)` | Convert `IPAddress` + port to `SockAddr`/`SockAddr6` in buffer |
| `PrepareBindAddress(isIPv6, port, buffer)` | Create wildcard bind address (`INADDR_ANY` / `in6addr_any`) |
| `GetAddressFamily(ip)` | Return `AF_INET` or `AF_INET6` |

## Socket Class

RAII TCP socket — move-only, stack-only, non-copyable.

### Lifecycle

```
Socket::Create(ip, port)     → allocate OS socket handle
  │
  socket.Open()              → TCP three-way handshake (5s timeout)
  │
  socket.Write(data)         → send data (loops for partial writes)
  socket.Read(buffer)        → blocking read (platform-specific timeout)
  │
  socket.Close()             → release handle, FIN on UEFI
```

### Methods

| Method | Signature | Purpose |
|---|---|---|
| `Create` | `static Create(ip, port) → Result<Socket, Error>` | Create TCP socket |
| `Open` | `Open() → Result<void, Error>` | Connect with 5-second timeout |
| `Close` | `Close() → Result<void, Error>` | Close connection |
| `Read` | `Read(Span<CHAR>) → Result<SSIZE, Error>` | Read data (returns 0 on peer close) |
| `Write` | `Write(Span<const CHAR>) → Result<UINT32, Error>` | Write all data (internal loop) |
| `IsValid` | `IsValid() → BOOL` | Handle valid? |
| `GetFd` | `GetFd() → SSIZE` | Raw file descriptor / handle |

### Timeouts

| Operation | Windows | POSIX | UEFI |
|---|---|---|---|
| Connect | 5 seconds | 5 seconds (ppoll/poll) | 5 seconds |
| Read | 5 minutes | — (blocking) | 60 seconds |
| Write | 1 minute/chunk | — (blocking) | 30 seconds |

## Platform Implementations

### POSIX

```
Create: socket(AF_INET/6, SOCK_STREAM, IPPROTO_TCP) → fd
Open:   fcntl(F_SETFL, O_NONBLOCK)
        connect(fd, addr, len)       // returns -EINPROGRESS
        ppoll/poll(fd, POLLOUT, 5s)  // wait for connection
        getsockopt(SO_ERROR)         // check result
        fcntl(F_SETFL, ~O_NONBLOCK)  // back to blocking
Read:   recvfrom(fd, buf, len, 0, NULL, NULL)
Write:  sendto(fd, buf, len, 0, NULL, 0)   // loops for partial
Close:  close(fd)
```

**Linux i386 special case:** Uses `socketcall(2)` multiplexer instead of direct socket syscalls. All socket operations are dispatched via sub-numbers (`SOCKOP_SOCKET`, `SOCKOP_CONNECT`, etc.) through a single `int $0x80` call.

### Windows (AFD Driver)

Windows networking bypasses Winsock entirely by talking directly to the AFD (Ancillary Function Driver):

```
Create: ZwCreateFile("\\Device\\Afd\\Endpoint",
                     EaBuffer = { AfdOpenPacket, family, type, protocol })
Open:   IOCTL_AFD_BIND(wildcard address)
        IOCTL_AFD_CONNECT(target address, 5s timeout via ZwWaitForSingleObject)
Read:   IOCTL_AFD_RECV(buffer, flags)
Write:  IOCTL_AFD_SEND(buffer, flags)
Close:  ZwClose(handle)
```

The AFD endpoint is opened as a file via `ZwCreateFile` with extended attributes specifying the socket parameters. All operations use `ZwDeviceIoControlFile` with AFD-specific IOCTL codes.

### UEFI (TCP Protocol Stack)

```
Create: LocateProtocol(EFI_TCP4_SERVICE_BINDING_PROTOCOL_GUID)
        ServiceBinding→CreateChild(&handle)
        HandleProtocol(handle, EFI_TCP4_PROTOCOL_GUID) → tcp
Open:   tcp→Configure(AccessPoint: { localAddr, localPort, remoteAddr, remotePort })
        tcp→Connect(&token)    // async
        poll for completion (5s)
Read:   tcp→Receive(&token)    // async
        poll for completion (60s)
Write:  tcp→Transmit(&token)   // async
        poll for completion (30s)
Close:  tcp→Close(&token)      // graceful FIN
        ServiceBinding→DestroyChild(handle)
```

UEFI networking is fully asynchronous — each operation creates a completion token and polls for completion.

## Platform Support

| Platform | IPv4 | IPv6 | Non-blocking Connect | Notes |
|---|---|---|---|---|
| Windows | Yes | Yes | Via AFD timeout | AFD driver, no Winsock |
| Linux | Yes | Yes | fcntl + ppoll | i386 uses socketcall multiplexer |
| Android | Yes | Yes | fcntl + ppoll | Same as Linux |
| macOS | Yes | Yes | fcntl + poll | BSD sockets |
| iOS | Yes | Yes | fcntl + poll | BSD sockets |
| FreeBSD | Yes | Yes | fcntl + poll | BSD sockets |
| Solaris | Yes | Yes | fcntl + poll | SVR4 socket numbers |
| UEFI | Yes | Yes | Async + poll | EFI TCP4/TCP6 protocols |
