# Android Platform Audit — upstream `pokemon-za`

Audit date: 2026-11-06

## Evidence inspected

The following files were fetched from the upstream `Hexveil404/PokeZ-ARecomp` repository, branch `pokemon-za`:

- `runtime/platform.h` — host platform API contract.
- `runtime/platform/linux.c` — Linux implementation.
- `runtime/platform/linux_compat.h` — Linux/glibc compatibility helpers.
- `runtime/main.c` — startup, launcher, game boot and reboot flow.
- `runtime/paths.h` — data-root and resource-path contract.
- `Makefile` — platform selection, source lists, compiler flags and dependencies.

This is a source inspection, not a successful Android build.

## Confirmed blockers

### 1. The Makefile has no Android platform branch

The Makefile selects `windows`, `linux`, or `macos`. Its runtime source list includes exactly:

```make
runtime/platform/$(PLATFORM).c
```

The Linux branch also chooses Linux-specific libraries and Vulkan backend flags. The generic `else` branch is the Windows build configuration. Therefore an Android target cannot safely be obtained by merely setting `PLATFORM=android`: platform selection, flags, libraries, backend files and packaging all need an explicit Android branch.

### 2. The platform API is broader than a few POSIX calls

`runtime/platform.h` requires implementations for:

- Guest address reservation, reset, commit, protection and optional write tracking.
- Monotonic and thread CPU clocks, timed sleep, CPU count and scheduling hints.
- Process/system information, executable path, relaunch/exec and termination.
- Child-process spawn/wait/kill, recursive removal, symlink checks, file open and rename.
- Symbol/module lookup, backtraces, crash handlers, fatal notices and diagnostics.

An Android implementation must satisfy this interface or deliberately redesign call sites that rely on desktop process behavior.

### 3. Linux implementation uses facilities that cannot be assumed on Android

The inspected Linux source includes Linux-specific headers and uses `userfaultfd`, `PAGEMAP_SCAN`, `/proc/self/pagemap`, `sched_getaffinity`, process priority controls, POSIX signals and desktop process/logging assumptions. Android shares the Linux kernel but has a different userspace, Bionic libc, app sandbox and SELinux policy. Each facility must be checked on the target Android API/device. In particular, do not assume that userfaultfd/PAGEMAP_SCAN or unrestricted process spawning is available.

The existing Linux code itself treats write tracking as optional and can return `-1` when tracking is unavailable. An Android port should initially use the documented unsupported path for optional tracking rather than depend on privileged kernel features.

### 4. Runtime startup assumes a desktop process and writable folder

`runtime/main.c` calls `plat_init`, opens logs, changes the data root, starts the launcher/game and uses process environment variables for test/relaunch behavior. It also contains a Linux-only early Vulkan/window initialization path. Android needs a lifecycle-aware entry point and must not assume the current working directory or executable directory is writable.

### 5. Data paths need app-private storage

`runtime/paths.h` defines a data root containing extracted game data, saves, shader/pipeline caches, settings and imported fonts. Android should map this to the app's private files/cache directories. User-selected documents must be accessed through Android's Storage Access Framework (SAF); a content URI must not be treated as a normal filesystem path.

### 6. Graphics and UI need Android integration

The upstream project uses SDL3 and a Vulkan renderer on Linux/Windows. Reusing the Vulkan renderer does not supply an Android Activity, Android Vulkan surface, lifecycle-safe swapchain recreation, Android packaging, or touch UI. These must be integrated and tested against the phone's actual Vulkan driver.

## Recommended implementation order

1. Add an explicit Android platform/build target without changing existing desktop targets.
2. Create a minimal SDL3 Android Activity/application shell and verify pause/resume/surface recreation.
3. Implement the Android platform API in a dedicated `runtime/platform/android.c`; use app-private paths and Android logging.
4. Start with optional write tracking disabled; only add a supported implementation if device tests justify it.
5. Replace desktop helper-process assumptions with in-process Android-compatible helpers or a deliberate Android workflow.
6. Bring up Vulkan surface/swapchain and query real device features.
7. Integrate SAF file selection and local import/seekable access.
8. Only after the shell and runtime pass, attempt user-owned game-data setup and gameplay.

## Do not implement by copying Linux wholesale

The Linux backend is a useful reference for semantics, but it is not an Android backend. Avoid copying `linux.c` and renaming it. Keep Android-specific code isolated and preserve desktop build behavior.

## Verification still required

- Exact Android NDK/API level and ABI selection.
- Phone SoC/GPU, Vulkan API version, extensions and driver behavior.
- Whether exact guest address reservation/protection works in the app process.
- Availability of required memory mapping flags and page sizes.
- SDL3 Android integration version and lifecycle behavior.
- Android-compatible builds of FFmpeg, shaderc, Vulkan Memory Allocator, ImGui and other dependencies.
- Compiler/generator/helper toolchain compatibility on Android ARM64.
- Real device build, launch, memory, thermal and gameplay tests.

## Milestone result

**Audit completed at source-inspection level. Android implementation and APK build are not started.** The next coding milestone should be the isolated Android application/NDK shell plus a compile-tested subset of the platform layer—not an unverified full-game port.
