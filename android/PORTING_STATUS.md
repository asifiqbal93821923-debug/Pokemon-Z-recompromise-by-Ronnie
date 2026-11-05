# Android Port Status

Last updated: 2026-11-06

## Overall state

**Native Android probe APK built successfully in GitHub Actions.** This is a toolchain/JNI smoke-test app only, not a playable Pokémon Z-A port. No physical-device installation or gameplay test has been performed.

## Milestones

| Milestone | Status | Evidence / next action |
|---|---|---|
| Create project repository | Done | Public repository, default branch `main` |
| Add initial project documentation | Done | README, plan, source index and status tracker |
| Audit upstream platform abstraction | Done (source inspection) | `docs/ANDROID_PLATFORM_AUDIT.md`; inspected upstream platform API, Linux backend, main startup, paths and Makefile |
| Android Gradle/NDK shell | Done (smoke-test scope) | Java Activity + JNI C probe, arm64-v8a Gradle/CMake config |
| Android CI build | **Passed** | Run [#7](https://github.com/asifiqbal93821923-debug/Pokemon-Z-recompromise-by-Ronnie/actions/runs/42094435912); build and artifact upload steps succeeded |
| APK artifact | **Uploaded and inspected** | Artifact `pokemon-za-android-probe-arm64-debug`; contains `app-debug.apk` (3,809,344 bytes), recognized as an Android package |
| Android platform backend | Not started | Implement a distinct Android platform backend after runtime integration design |
| SDL3 Android integration | Not started | Integrate SDL3 Android Activity/lifecycle and multi-touch events |
| Vulkan Android surface | Not started | Create Android surface/swapchain path and inspect actual device Vulkan capabilities |
| Local game-file picker | Design exists separately | Integrate and test SAF descriptor handling |
| Device launch test | Not started | Install and exercise probe on owner's Android 16 device |
| Game setup/gameplay | Not started | Only after runtime, renderer and user-owned game-data setup work |

## Target device information

- Android: 16 (reported)
- RAM: 8 GB (reported)
- Storage: 128 GB (reported)
- SoC/GPU: not verified
- Vulkan version/extensions: not verified
- Device installation/test: not performed

## Verified smoke-build scope

The CI job used JDK 17, Gradle 9.6.0, Android SDK 36, NDK 28.2 and CMake 3.22.1. It compiled the arm64-v8a debug APK and uploaded the artifact. The APK contains only the minimal Java Activity and JNI C probe. It does **not** include SDL3, Vulkan rendering, the ZA Recomp runtime, a game importer or game data.

## Upstream Android-port constraints

The upstream Makefile currently selects Windows/Linux/macOS and compiles `runtime/platform/$(PLATFORM).c`. The Linux backend relies on Linux-specific facilities and desktop process/path assumptions; Android needs a dedicated implementation rather than renaming or copying the Linux backend. See `docs/ANDROID_PLATFORM_AUDIT.md` and `android/ANDROID_PLATFORM_CONTRACT.md`.

## Rules for status updates

- "Implemented" means code exists.
- "Build passed" means a real build completed successfully and its output was inspected.
- "Installed" means the APK was installed on a device.
- "Tested" means the feature was exercised on the stated device.
- "Playable" means actual in-game gameplay was observed and recorded.
- Never infer a later status from an earlier one.
