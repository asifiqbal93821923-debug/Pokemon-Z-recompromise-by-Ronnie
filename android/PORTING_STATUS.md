# Android Port Status

Last updated: 2026-11-06

## Overall state

**The first arm64 JNI probe APK has built successfully. SDL3 integration source is now committed and its Android build is running in GitHub Actions.** Neither APK has been installed on the target phone. No game runtime or gameplay is included yet.

## Milestones

| Milestone | Status | Evidence / next action |
|---|---|---|
| Project repository and planning docs | Done | Public repository, branch `main` |
| Upstream platform audit | Done (source inspection) | `docs/ANDROID_PLATFORM_AUDIT.md` |
| Basic Android JNI/NDK probe | Build passed | CI run [#7](https://github.com/asifiqbal93821923-debug/Pokemon-Z-recompromise-by-Ronnie/actions/runs/42094435912); APK artifact uploaded |
| SDL3 source integration | Source scaffold committed | Pinned upstream `release-3.4.18`; Java SDLActivity source set; CMake builds SDL3 + `main` shared library |
| SDL3 Android smoke build | In progress | CI checks out SDL3 and builds the arm64 debug APK; verify Gradle/CMake result |
| SDL3 touch/event loop | Implemented in probe source; build pending | `android/app/src/main/cpp/sdl_probe.c`; logs finger down/up/motion and changes diagnostic screen color |
| Android Vulkan surface/swapchain | Not started | Next after SDL3 build passes |
| Android runtime platform backend | Not started | Dedicated `runtime/platform/android.c) required; do not rename Linux backend |
| SAF game-file import | Design exists separately | Integrate descriptor bridge and local-copy fallback |
| Target-device install test | Not started | Requires APK installation on owner's Android 16 phone |
| Game data setup/gameplay | Not started | Only after native runtime and renderer work |

## Target device (reported)

- Android 16
- RAM 8 GB; storage 128 GB
- SoC/GPU and Vulkan extensions not verified

## SDL3 probe scope

The SDL3 smoke app uses the official SDL Android Activity glue and a native SDL event/render loop. It is intended to test Activity startup, video initialization, renderer creation, touch event delivery, and shutdown. It is not the ZA Recomp runtime and does not include Vulkan swapchain code, game assets, importer, or final on-screen controls.

SDL3 is pinned to upstream tag `release-3.4.18` in CI. The integration approach follows the official SDL3 Android README: https://wiki.libsdl.org/SDL3/README-android.

## Rules for status updates

- "Implemented" means code exists.
- "Build passed" means a real build completed successfully and its output was inspected.
- "Installed" means the APK was installed on a device.
- "Tested" means the feature was exercised on the stated device.
- "Playable" means actual in-game gameplay was observed and recorded.
- Never infer a later status from an earlier one.
