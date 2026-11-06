# Android Platform API Gap Matrix

Audited against upstream `Hexveil404/PokeZ-ARecomp`, branch `pokemon-za`, pinned commit `dfb47e9d8130d6ecde1c0afa1010fb1a102e50f6`.

This document is an implementation plan. It is not evidence that an Android backend has been implemented.

## Platform API audit

| Upstream API group | Upstream contract | Android implementation decision | Current status |
|---|---|---|---|
| Guest address window | Reserve exact `[lo, hi)`; lazy backing; reset entire range | Dedicated Bionic implementation; exact-address safety test first; preserve reservation ownership | Not device-tested |
| Commit / protect | Commit range; change page protection | `mmap`/ `mprotect` only after exact page alignment and range validation | Not implemented |
| Written-page tracking | Windows write-watch; optional Linux userfaultfd + PAGEMAP_SCAN | Android 16 / kernel 6.1.157: `userfaultfd` opens, combined `WP_ASYNC | WP_UNPOPULATED` API negotiation returns `EINVAL`; return unsupported (`-1`) until an isolated supported path or safe fallback is device-tested | Native tracking unsupported on current test; backend not implemented |
| Page state / memory usage | Optional diagnostic state and process/guest memory figures | Return unknown where Android cannot report reliably; no fabricated metrics | Not implemented |
| Monotonic timing | Nanosecond monotonic clock and deadline sleep | Android clock APIs / `clock_nanosleep` availability to be checked with NDK; use monotonic clock | Not implemented |
| Thread scheduling | Guest priority mapping, realtime pacing and CPU time | Conservative pthread scheduling; gracefully handle Android permission/policy denial; no assumption of realtime privilege | Not implemented |
| CPU count / system info | CPU, core/thread, RAM and OS strings | Android system properties / sysconf; avoid exposing private user identifiers | Not implemented |
| Process lifecycle | `plat_init`, executable path, `plat_exec`, terminate | Android Activity/SDL process owns lifecycle. Upstream constructor-time reservation and re-exec-on-collision must be redesigned; do not call desktop re-exec blindly | Not implemented |
| Child processes | Spawn, wait, kill helper processes | Decide which helpers can become in-process library calls or worker components; executable child-process behavior requires separate Android validation | Not implemented |
| Filesystem | remove tree, symlink detection, open, rename | App-private filesystem for writable data; SAF URI/descriptor bridge for user-selected files; no assumption that content URI is a POSIX path | Not implemented |
| Diagnostics | symbol/module lookup, backtrace, crash notice, signal handlers | NDK-compatible backtrace and tombstone/log integration; UI notice must be marshalled to Java main thread | Not implemented |
| Window/screen state | Desktop screen hash / HWND-style information | Not applicable on Android; return false / omit | Not implemented |
| Vulkan/SDL surface | Upstream SDL Vulkan renderer and device code | Use SDL Android surface lifecycle; rebuild swapchain on surface loss/size change; current probe proves only first-frame presentation | Probe only |

## Critical runtime blockers

### 1. Constructor-time address reservation

Upstream `runtime/main.c` reserves the guest window from a constructor before `main()`. If the address is unavailable, desktop code attempts to re-exec itself. Android must reserve the range in the SDL process before guest-runtime initialization, and collision recovery must be designed around Android process/activity lifecycle. A diagnostic probe must not be mistaken for a complete solution.

### 2. Optional Linux write tracking

The Linux backend's write tracking relies on `userfaultfd` and `PAGEMAP_SCAN` (Linux 6.7+). Build #38 tested the target device: `userfaultfd()` with `UFFD_USER_MODE_ONLY` opens, but requesting `UFFD_FEATURE_WP_ASYNC | UFFD_FEATURE_WP_UNPOPULATED` in `UFFDIO_API` returns `EINVAL`. `WP_ASYNC` was introduced in Linux 6.7, later than this device's 6.1.157 base; vendor backports remain possible, so test each feature on a fresh descriptor and query the negotiated feature mask before concluding. `PAGEMAP_SCAN` was not reached or tested. The Android backend must gracefully return unsupported (`-1`) unless a safe, measured mechanism exists; never use unsafe global signal handlers as a substitute.

#### 2a. Candidate fallback investigation

A possible fallback is soft-dirty tracking through `/proc/self/clear_refs` and the soft-dirty bit in `/proc/self/pagemap`. This is not yet validated on the target Android build. Clearing soft-dirty is process-wide rather than limited to the guest mapping, so it may interfere with other runtime bookkeeping; measure access permissions, correctness, scan cost across the 32-GiB range, and coordination requirements before adopting it. If those costs or semantics are unacceptable, keep write tracking unsupported.

## 3. Filesystem and SAF

The runtime expects ordinary filesystem paths for extracted files, saves, settings, caches and logs. Android Storage Access Framework returns content URIs and file descriptors, not necessarily stable filesystem paths. Implement a copy/import bridge into app-private storage with explicit user consent; never pretend a content URI is a path.

### 4. Helper processes

Upstream uses child processes for tools such as seed generation and game-file extraction. The Android design must inventory each helper and decide whether it is built as an in-process library, a supported app-private executable, or a separate Android component. Do not assume desktop `posix_spawn` behavior.

### 5. Vulkan lifecycle

Build #30 confirms Vulkan instance, Android surface, graphics/present queues, logical device, swapchain and one blue first frame on the target device. It does not validate upstream NVN emulation, descriptor/pipeline features, guest GPU memory mapping, shader compilation, FSR2, swapchain recreation after pause/resume, or sustained frame pacing.

## Acceptance order

1. Compile a minimal Android backend skeleton against the complete upstream `platform.h` API, without linking the game runtime.
2. Add unit-style tests for time, file operations, page protection and error reporting.
3. Validate guest-window reservation/reset in the actual SDL process; record exact stage and errno.
4. Implement lifecycle-safe SDL/Vulkan surface recreation and test repeated background/foreground cycles.
5. Add app-private data root and SAF import using a harmless test file.
6. Integrate upstream runtime sources in a pinned, separate patch series and resolve linker/compiler gaps.
7. Only then attempt a private local runtime boot with user-owned game data.

## Explicit non-goals

- No proprietary game data, keys, firmware, extracted assets, saves, generated game code or copyrighted material in Git.
- No claim of gameplay or compatibility from a diagnostic APK.
- No automatic game execution or trade-like unattended actions.
