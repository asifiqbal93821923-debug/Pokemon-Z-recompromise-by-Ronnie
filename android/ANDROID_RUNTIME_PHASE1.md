# Phase 1 — Android Runtime Platform Integration

## Current result

The first Android runtime-platform foundation is implemented and CI-built. The native Android ARM64 library now compiles and links the Android platform adapter together with the SDL diagnostic library. The link probe references all 39 common platform functions applicable to Android from the pinned upstream platform header.

This is a platform-adapter milestone, not a completed game runtime. Phase 1 remains open until the upstream runtime itself is brought into the Android target and its Android lifecycle/process assumptions are resolved.

## Completed in this milestone

- Bionic guest-memory reservation, reset, protection and lazy-commit contract.
- Safe unavailable result for write tracking, with renderer AUTO fallback policy documented.
- Monotonic/thread CPU clocks, absolute sleep, CPU availability and conservative scheduling hints.
- Android system information and expected-unsupported APK executable-path behavior.
- SDL app-private preference-path bootstrap into XDG_DATA_HOME before upstream paths initialization.
- App-private POSIX file operations, recursive removal that does not follow symlinks, and no-overwrite rename.
- Android logcat diagnostics, symbol/module lookup, native backtrace, notice handling and AArch64 signal-context forwarding to upstream fault handlers when those are linked.
- Explicit ENOTSUP behavior for desktop process-replacement and helper-process operations.
- Link-time API coverage probe and on-device runtime-environment assertion.

## CI evidence

- Commit: 4ff28e326597798f2ba329fbf6066e6a4054054a
- GitHub Actions run: 42217357351
- Result: success for the Android ARM64 debug diagnostic target.
- This proves the current Android platform adapter and probe compile and link. It does not prove the upstream runtime starts.

## Phone verification required

The next side-by-side APK checks:
- ZA_ANDROID_RUNTIME_ENV result=PASS
- linked_platform_symbols=39
- app_private_xdg_data_home=yes
- prior guest-memory, SDL3, touch and Vulkan diagnostic checks still pass.

No large NSP re-import is required. The existing imported file stays in the separate recovery app's private storage.

## Remaining blockers inside Phase 1

1. The repository mirror currently contains only the upstream platform header and Android compatibility shim, not the full upstream runtime source tree.
2. The upstream desktop Makefile has no Android platform target. An Android-specific runtime build graph and dependency integration are still needed.
3. The current SDL Activity is a diagnostic Activity, not the actual runtime entry point. Pause/resume, window-surface recreation and process-death recovery must be integrated with the runtime.
4. plat_spawn / plat_proc_wait / plat_proc_kill and plat_exec / plat_relaunch deliberately return unsupported. The desktop launcher/compiler/helper-process workflow must be replaced with Android-compatible in-process libraries or Android components.
5. The real Vulkan renderer and its AUTO fallback are not linked or executed by this repository's current diagnostic target.

## Phase 1 completion gate

Phase 1 can be marked complete only after:
- the pinned upstream runtime sources are part of the Android build;
- an Android ARM64 runtime library/executable target compiles and links against the backend;
- app-private data paths and lifecycle are wired into the actual runtime entry point;
- unsupported helper-process assumptions are removed from the Android launch path;
- a runtime-without-game-data smoke launch succeeds on the target phone.

This phase does not include game-data decryption, game-specific generated code, or playable gameplay. Those belong to later phases and require the user's own legally obtained data outside GitHub.


## Upstream runtime core Android compile gate

The Android NDK build now compiles the pinned upstream runtime's root C/C++ sources plus HLE and service C sources into a static compile-check archive. Desktop platform translation units are excluded. This is not a runnable game runtime and does not include generated game code, NVN/Vulkan renderer sources, or UI Objective-C++ files; it exposes Bionic/source compatibility errors in the core runtime before attempting renderer integration.

The upstream platform header includes a glibc-only Linux compatibility header when __linux__ is set. An Android-only CI patch now excludes that glibc shim when __ANDROID__ is set. The patch is applied only in the checkout used by CI; the pinned upstream submodule commit remains unchanged.


## Android runtime path-manager smoke integration

The native diagnostic now calls the pinned upstream rt_paths_init() / rt_data_root() APIs after the Android platform bootstrap. The Android linker pulls the actual upstream runtime/paths.c object from the compiled core archive into libmain.so. On the target phone, the test asserts that the chosen runtime root is exactly the app-private XDG data base plus zarecomp, exists as a directory, and is readable/writable.

This exercises one real upstream runtime subsystem through the Android backend. It does not yet start runtime/main.c, boot a game, use the Vulkan renderer, or launch generated game code.


The Android-only upstream patch also enlarges the POSIX realpath output buffers in gamebuild.c and main.c to PATH_MAX. This is required by Bionic's compile-time realpath buffer contract; Windows buffers and code paths remain unchanged.


### Reproducible Android/Bionic source edits

CI applies narrowly scoped source edits from android/scripts/patch_upstream_for_android.py rather than a hand-maintained unified diff. The script checks that each upstream anchor occurs exactly once and verifies the final source. It excludes the glibc Linux shim on Android and sizes POSIX realpath output buffers to PATH_MAX in gamebuild.c and main.c, while leaving Windows branches unchanged.


## Android arm64 fiber backend

Android Bionic does not expose POSIX getcontext/makecontext/swapcontext. The Android-only source adaptation replaces those calls in the pinned upstream runtime/hle/fiber.c with an AArch64 cooperative context switch. It preserves x19–x30, SP, d8–d15, FPCR and FPSR according to the Android ARM64 ABI. Windows and desktop POSIX branches remain unchanged.

The Android NDK build includes the assembly backend and a native context-switch probe. The probe verifies switching from the calling context to a new stack and resuming twice. This is a focused ABI smoke test; full multi-thread fiber scheduling and game workload stress still require device/runtime testing.


### Deliberate compile boundary: movie HLE

The core compile-check excludes upstream runtime/hle/movie.c because that source requires FFmpeg headers and libraries (libavcodec, libavformat, libavutil and libswscale), which are not yet integrated as Android ARM64 prebuilts. No stub or fake video support is added. Android FFmpeg integration and movie playback remain explicit runtime dependencies outside this Phase 1 platform/core gate.


### Android realtime clock compatibility

The upstream system-clock HLE uses C11 timespec_get(TIME_UTC), which is not exposed by the current Android Bionic headers in this build configuration. The Android-only source adaptation uses clock_gettime(CLOCK_REALTIME) with a zero-initialized timespec. It retains wall-clock/realtime semantics; it is not replaced with a monotonic clock.
