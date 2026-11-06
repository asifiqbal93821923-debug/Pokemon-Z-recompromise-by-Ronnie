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
