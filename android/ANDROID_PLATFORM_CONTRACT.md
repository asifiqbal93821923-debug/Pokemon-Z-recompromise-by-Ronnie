# Android Platform Layer — Implementation Contract

This file translates the pinned upstream runtime/platform.h interface into Android implementation requirements. It is a design checklist, not an implementation.

## Runtime contract

| API area | Android approach | Initial support policy |
|---|---|---|
| Guest address space | NDK mmap/mprotect; validate exact range and collision behavior | Required; fail safely if exact range cannot be reserved |
| Commit/reset | Preserve guest memory semantics and page alignment | Required |
| Written-page tracking | Use UFFD async WP only if advertised and validated | Return -1 on the tested device; renderer has a conservative full-range-copy fallback |
| Memory usage/page state | Use supported process metrics only | Unknown/false where unavailable |
| Monotonic time | clock_gettime(CLOCK_MONOTONIC) | Required |
| Sleep | Android/Bionic-compatible absolute sleep; handle interruption | Required |
| CPU count | sysconf and Android affinity information | Best effort |
| Thread priority | Android-supported pthread scheduling; no privileged realtime assumption | Best effort |
| Data root | App-private files/cache directories | Required |
| Child processes | Redesign desktop helper process model | Unsupported until explicitly implemented |
| File operations | POSIX/Bionic in app-private storage; SAF for user documents | Required |
| Backtrace/crash | Android logcat and supported NDK crash mechanism | Best effort |
| Fatal notice | Android Activity/UI bridge | Required for user-facing errors |

## Build integration requirements

- Add a dedicated Android NDK target; leave desktop targets unchanged.
- Select arm64-v8a explicitly.
- Build Android-compatible dependencies.
- Integrate SDL3 Activity and Android Vulkan surface lifecycle.
- Keep game-specific generated code and user game data outside the repository.

## Lifecycle requirements

- Pause/resume must safely suspend/resume rendering and audio.
- Surface loss/recreation must rebuild swapchain resources.
- Activity recreation and process death must not corrupt saves/imports.
- Touch must support simultaneous pointers and route through SDL3 input.

## Device evidence — Build #39

OnePlus CPH2943 / Android 16 / arm64-v8a:

- SDL3 window/event loop, Vulkan instance/surface/device/queues/swapchain and first blue frame PASS.
- 32-GiB guest virtual address reservation and one-page protection sequence PASS.
- Multi-pointer touch events observed.
- UFFD opens and feature query succeeds; advertised mask 0x10ffe omits PAGEFAULT_FLAG_WP, WP_ASYNC, and WP_UNPOPULATED.
- Soft-dirty is unsupported on AArch64; bit 55 is not a valid fallback.
- Original PAGEMAP_SCAN EACCES came from a probe that did not establish WP prerequisites. Build #40 will test generic PAGE_IS_PRESENT separately and only query PAGE_IS_WRITTEN after valid WP setup.
- No dangerous Android runtime permissions are requested. Kernel ioctl denial is not a user permission decline.

The Build #39 Activity sequence (create/start/resume → pause/stop → start/resume) is consistent with normal background/foreground transitions. It is not evidence of a crash. Surface recreation remains untested.

This validates diagnostic paths only, not the upstream renderer, complete guest-memory semantics, or game runtime.
