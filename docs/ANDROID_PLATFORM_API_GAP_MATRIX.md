# Android Platform API Gap Matrix

Audited against upstream `Hexveil404/PokeZ-ARecomp`, branch `pokemon-za`, pinned commit `dfb47e9d8130d6ecde1c0afa1010fb1a102e50f6`.

This document is an implementation plan. It is not evidence that an Android backend has been implemented.

## Platform API audit

| Upstream API group | Upstream contract | Android implementation decision | Current status |
|---|---|---|---|
| Guest address window | Reserve exact [lo, hi); lazy backing; reset entire range | Dedicated Bionic implementation; exact-address safety test first; preserve reservation ownership | Exact 32-GiB reserve and release device-tested PASS |
| Commit / protect | Commit range; change page protection | `mmap`/`mprotect` only after exact page alignment and range validation | One-page commit/readback/protection smoke test PASS; full backend not implemented |
| Written-page tracking | Windows write-watch; optional Linux userfaultfd + PAGEMAP_SCAN | Return unsupported (-1) on this device unless a safe, measured alternative is proven | Build #39: userfaultfd opens/API query succeeds, but WP_ASYNC and WP_UNPOPULATED are not advertised; PAGEMAP_SCAN returns EACCES; soft-dirty bit 55 was not set in the one-page probe |
| Page state / memory usage | Optional diagnostic state and process/guest memory figures | Return unknown where Android cannot report reliably; no fabricated metrics | Not implemented |
| Monotonic timing | Nanosecond monotonic clock and deadline sleep | Android clock APIs / `clock_nanosleep` availability to be checked with NDK; use monotonic clock | Diagnostic monotonic clock read PASS; platform contract not implemented |
| Thread scheduling | Guest priority mapping, realtime pacing and CPU time | Conservative pthread scheduling; gracefully handle Android permission/policy denial; no assumption of realtime privilege | Not implemented |
| CPU count / system info | CPU, core/thread, RAM and OS strings | Android system properties / sysconf; avoid exposing private user identifiers | Diagnostic device/CPU reporting available; platform contract not implemented |
| Process lifecycle | `plat_init`, executable path, `plat_exec`, terminate | Android Activity/SDL process owns lifecycle. Upstream constructor-time reservation and re-exec-on-collision must be redesigned; do not call desktop re-exec blindly | Not implemented |
| Child processes | Spawn, wait, kill helper processes | Decide which helpers can become in-process library calls or worker components; executable child-process behavior requires separate Android validation | Not implemented |
| Filesystem | remove tree, symlink detection, open, rename | App-private filesystem for writable data; SAF URI/descriptor bridge for user-selected files; no assumption that content URI is a POSIX path | Not implemented |
| Diagnostics | symbol/module lookup, backtrace, crash notice, signal handlers | NDK-compatible backtrace and tombstone/log integration; UI notice must be marshalled to Java main thread | Diagnostic log capture exists; platform contract not implemented |
| Window/screen state | Desktop screen hash / HWND-style information | Not applicable on Android; return false / omit | Not implemented |
| Vulkan/SDL surface | Upstream SDL Vulkan renderer and device code | Use SDL Android surface lifecycle; rebuild swapchain on surface loss/size change; current probe proves only first-frame presentation | Probe only; first-frame presentation device-tested PASS |

## Critical runtime blockers

### 1. Constructor-time address reservation

Upstream `runtime/main.c` reserves the guest window from a constructor before `main()`. If the address is unavailable, desktop code attempts to re-exec itself. Android must reserve the range in the SDL process before guest-runtime initialization, and collision recovery must be designed around Android process/activity lifecycle. The diagnostic probe's exact 32-GiB `mmap` reservation is evidence for that isolated call only, not a complete runtime solution.

### 2. Optional Linux write tracking

The upstream Linux backend's write tracking uses `userfaultfd` asynchronous write protection and `PAGEMAP_SCAN`. The upstream implementation documents Linux 6.7+ for this path. The target phone reports Android 16 with kernel `6.1.157-android14-11-o-gdeed0ebc8d5d`.

Build #39 isolated the kernel capabilities on the actual device:

- `userfaultfd()` with `UFFD_USER_MODE_ONLY`: opens successfully.
- `UFFDIO_API` feature query with `features=0`: succeeds; advertised mask `0x10ffe`.
- `UFFD_FEATURE_WP_ASYNC`: not advertised.
- `UFFD_FEATURE_WP_UNPOPULATED`: not advertised.
- `/proc/self/pagemap`: readable for the one-page diagnostic probe.
- `/proc/self/clear_refs`: write of soft-dirty clear command succeeded far enough for the subsequent pagemap read; after a write, bit 55 was still not set in the returned entry (`0x8100000000000000`).
- `PAGEMAP_SCAN`: ioctl failed with `EACCES` (permission denied). This is not proof that the ioctl is absent; the permission boundary prevented a capability conclusion.

Therefore the upstream asynchronous write-watch path cannot be enabled on this tested device. The soft-dirty probe also did not demonstrate a usable fallback. The Android backend should return unsupported (`-1`) from `plat_written_pages()` for now. Do not add a process-wide signal handler or assume soft-dirty works. The upstream platform contract explicitly allows `-1` where write tracking is unavailable.

#### 2a. Fallback decision

Do not adopt soft-dirty tracking based on this result. The single-page test did not observe bit 55 after clearing and writing; even if a future Android build exposes it, clearing soft-dirty is process-wide and a 32-GiB scan may be expensive. Revisit only with a reliable correctness test, permission review, whole-range cost measurement, and synchronization design. A software-instrumented write-barrier path may be researched separately, but must be validated against the generated guest memory access code before implementation.

## 3. Filesystem and SAF

The runtime expects ordinary filesystem paths for extracted files, saves, settings, caches and logs. Android Storage Access Framework returns content URIs and file descriptors, not necessarily stable filesystem paths. Implement a copy/import bridge into app-private storage with explicit user consent; never pretend a content URI is a path.

## 4. Helper processes

Upstream uses child processes for tools such as seed generation and game-file extraction. The Android design must inventory each helper and decide whether it is built as an in-process library, a supported app-private executable, or a separate Android component. Do not assume desktop `posix_spawn` behavior.

## 5. Vulkan lifecycle

Build #39 owner log reconfirms Vulkan instance, Android surface, Mali-G615 MC2 physical device, graphics/present queues, logical device, 2400×1080 swapchain with six images, and a blue first frame. It does not validate upstream NVN emulation, descriptor/pipeline features, guest GPU memory mapping, shader compilation, FSR2, swapchain recreation after pause/resume, or sustained frame pacing.

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
- No automatic game execution or unattended actions.
