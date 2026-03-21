# Screen Capture

Platform-independent display enumeration and framebuffer capture for screen recording and remote display.

## File Map

```
screen/
├── screen.h               # Interface: ScreenDevice, ScreenDeviceList, Screen class
├── windows/screen.cc      # Windows (User32 + GDI32 BitBlt/GetDIBits)
├── posix/screen.cc        # Linux/Android (DRM dumb buffers + fbdev fallback)
├── macos/screen.cc        # macOS (CoreGraphics via dyld framework loader)
├── ios/screen.cc          # iOS (stub — not implemented)
├── solaris/screen.cc      # Solaris (framebuffer device + FBIOGTYPE ioctl)
└── uefi/screen.cc         # UEFI (Graphics Output Protocol BLT)
```

## Data Structures

### ScreenDevice (packed)

```c
struct ScreenDevice {
    INT32 Left;           // X position on virtual desktop
    INT32 Top;            // Y position on virtual desktop
    UINT32 Width;         // Horizontal resolution in pixels
    UINT32 Height;        // Vertical resolution in pixels
    BOOL Primary;         // TRUE if primary display
};
```

### ScreenDeviceList

```c
struct ScreenDeviceList {
    ScreenDevice* Devices;   // Heap-allocated array
    UINT32 Count;            // Number of displays
    void Free();             // Deallocate array
};
```

## Screen Class

Static class — no instantiation.

| Method | Signature | Purpose |
|---|---|---|
| `GetDevices` | `static GetDevices() → Result<ScreenDeviceList, Error>` | Enumerate connected displays |
| `Capture` | `static Capture(device, Span<RGB> buffer) → Result<void, Error>` | Capture framebuffer pixels |

**Buffer requirement:** `Capture` expects a pre-allocated buffer of at least `Width * Height` RGB pixels (3 bytes per pixel, top-down, left-to-right).

## Platform Implementations

### Windows

**Display Enumeration:**
1. `User32::EnumDisplayDevicesW(NULL, i, &dev, 0)` — iterate adapters
2. Filter by `DISPLAY_DEVICE_ACTIVE` flag
3. `User32::EnumDisplaySettingsW(name, ENUM_CURRENT_SETTINGS, &mode)` — get resolution
4. Check `DISPLAY_DEVICE_PRIMARY_DEVICE` for primary flag

**Screen Capture:**
1. `User32::GetDC(NULL)` → screen device context
2. `Gdi32::CreateCompatibleDC(screenDC)` → memory DC
3. `Gdi32::CreateCompatibleBitmap(screenDC, width, height)` → bitmap
4. `Gdi32::SelectObject(memDC, bitmap)` → select into memory DC
5. `Gdi32::BitBlt(memDC, 0, 0, w, h, screenDC, x, y, SRCCOPY)` → copy pixels
6. `Gdi32::GetDIBits(memDC, bitmap, 0, h, buffer, &bmi, DIB_RGB_COLORS)` → extract
7. Cleanup: `DeleteObject`, `DeleteDC`, `ReleaseDC`

### Linux / Android

**Primary method: DRM dumb buffers** (`/dev/dri/card*`)
1. Open DRM device
2. Read display mode info from sysfs
3. `mmap` the framebuffer
4. Copy pixels to output buffer

**Fallback: fbdev** (`/dev/fb0` through `/dev/fb7`)
1. `ioctl(FBIOGET_FSCREENINFO)` — get fixed screen info (line length, framebuffer size)
2. `ioctl(FBIOGET_VSCREENINFO)` — get variable screen info (resolution, bpp)
3. `mmap` the framebuffer
4. Copy pixels, handle BPP conversion

### macOS

Uses **CoreGraphics framework** loaded dynamically via dyld:
1. `dlopen("CoreGraphics.framework")` via dyld resolution
2. `dlsym` for `CGGetActiveDisplayList`, `CGDisplayBounds`, `CGDisplayPixelsWide/High`
3. `CGDisplayCreateImage` for screenshot capture
4. Extract pixel data from `CGImage`

### Solaris

Uses **framebuffer device** (`/dev/fb`):
1. `ioctl(FBIOGTYPE)` — query display type and dimensions
2. `mmap` the framebuffer memory
3. Copy pixels to output buffer

### UEFI

Uses **Graphics Output Protocol** (GOP):
1. `LocateProtocol(EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID)` → GOP handle
2. `GOP→QueryMode` — get resolution and pixel format
3. `GOP→Blt(VideoToBltBuffer)` — copy framebuffer to buffer

### iOS

**Not implemented** — stub returns error. iOS restricts direct framebuffer access.

## Platform Support Summary

| Platform | Enumeration | Capture | Method |
|---|---|---|---|
| Windows | Multi-monitor | Full | GDI BitBlt + GetDIBits |
| Linux | Single/multi | Full | DRM dumb buffers / fbdev |
| Android | Single/multi | Full | DRM dumb buffers / fbdev |
| macOS | Multi-monitor | Full | CoreGraphics framework |
| iOS | — | — | Not implemented |
| Solaris | Single | Full | Framebuffer /dev/fb |
| FreeBSD | — | — | Uses POSIX (DRM/fbdev if available) |
| UEFI | Single | Full | GOP protocol |
