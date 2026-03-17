---
name: New Platform / Architecture Support
about: Request or propose support for a new platform or CPU architecture
title: "[Platform] "
labels: platform
assignees: ''
---

## Platform / Architecture

- **OS:** (e.g., NetBSD, OpenBSD, QNX)
- **Architecture:** (e.g., ppc64, loongarch64, s390x)

## Runtime Support Status

Does the runtime layer (`src/platform/`) already support this platform?

- [ ] Yes - platform layer exists, just needs build presets and CI
- [ ] No - platform layer must be added (kernel syscalls, memory, console, fs, socket, screen, system)

## Implementation Plan

Outline what needs to be implemented:

- [ ] CMake presets (`CMakePresets.json`)
- [ ] CI workflow (`.github/workflows/`)
- [ ] VSCode configuration (`.vscode/`)
- [ ] Platform-specific command handler adjustments (if any)
- [ ] README build matrix update

## References

Links to relevant documentation, man pages, or ABI specifications.

## Additional Context

Any other relevant information, known challenges, or related issues.
