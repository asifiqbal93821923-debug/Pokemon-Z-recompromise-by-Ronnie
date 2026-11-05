# Android Port Status

Last updated: 2026-11-06

## Overall state

<badge>Research and planning</badge>

No Android APK has been built from this repository. No Android device test has been performed from this repository. This file tracks work; checked items require evidence.

## Milestones

| Milestone | Status | Evidence / next action |
|---|---|---|
| Create project repository | Done | Public repository, default branch `main` |
| Add initial project documentation | Done | README, plan, source index and status tracker committed and fetched back |
| Audit upstream platform abstraction | Done (source inspection) | `docs/ANDROID_PLATFORM_AUDIT.md`; upstream `platform.h`, `linux.c`, `main.c`, `paths.h`, `Makefile` inspected |
| Android Gradle/NDK shell | Not started | Create a minimal arm64-v8a app; see `android/ANDROID_PLATFORM_CONTRACT.md` |
| Android platform backend | Not started | Implement only after NDK shell/build target exists; no Android C implementation committed yet |
| SDL3 Android integration | Not started | Activity lifecycle and multi-touch events |
| Vulkan Android surface | Not started | Query actual device features and test swapchain |
| Local game-file picker | Design exists separately | Integrate and test SAF descriptor behavior |
| Native build | Not started | Record exact build output |
| Device launch test | Not started | Test on owner's Android 16 device |
| Game setup/gameplay | Not started | Only after all previous milestones pass |

## Target device information

- Android: 16 (reported)
- RAM: 8 GB (reported)
- Storage: 128 GB (reported)
- SoC/GPU: not verified
- Vulkan version/extensions: not verified
- Device build/test: not performed

## Latest audit result\n\nThe upstream Makefile selects Windows/Linux/macOS only and compiles `runtime/platform/$(PLATFORM).c`; its Linux backend uses `userfaultfd`/`PAGEMAP_SCAN` and desktop process/path assumptions. Android therefore needs a distinct build branch and platform backend. Audit details: `docs/ANDROID_PLATFORM_AUDIT.md`.\n\n## Rules for status updates

- "Implemented" means code exists.
- "Build passed" means a real build completed successfully and its output was inspected.
- "Installed" means the APK was installed on a device.
- "Tested" means the feature was exercised on the stated device.
- "Playable" means actual in-game gameplay was observed and recorded.
- Never infer a later status from an earlier one.
