# Android Port Status

Last updated: 2026-11-06

## Overall state

<badge>Research and planning</badge>

No Android APK has been built from this repository. No Android device test has been performed from this repository. This file tracks work; checked items require evidence.

## Milestones

| Milestone | Status | Evidence / next action |
|---|---|---|
| Create project repository | Done | Public repository, default branch `main` |
| Add initial project documentation | In progress | This initial documentation commit |
| Audit upstream platform abstraction | Pending | Inspect exact upstream files and record API gaps |
| Android Gradle/NDK shell | Not started | Create a minimal arm64-v8a app |
| Android platform backend | Not started | Implement and compile platform API |
| SDL3 Android integration | Not started | Lifecycle and touch events |
| Vulkan Android surface | Not started | Device capability and swapchain test |
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

## Rules for status updates

- "Implemented" means code exists.
- "Build passed" means a real build completed successfully and its output was inspected.
- "Installed" means the APK was installed on a device.
- "Tested" means the feature was exercised on the stated device.
- "Playable" means actual in-game gameplay was observed and recorded.
- Never infer a later status from an earlier one.
