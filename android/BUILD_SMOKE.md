# Android NDK Smoke-Test Shell

## Purpose

This is the first minimal Android-native build target for the port workspace. It checks that a Java Activity can load an ARM64 JNI shared library and read basic device/kernel information.

It is **not** the ZA Recomp runtime and is **not** a game APK.

## Toolchain pinned for reproducibility

- Android Gradle Plugin: 9.4.1
- Gradle: 9.6.0
- JDK: 17
- Android compile/target SDK: 36
- Android NDK: 28.2.13676358
- CMake: 3.22.1
- ABI: arm64-v8a only
- Minimum Android: API 28

The AGP/Gradle/JDK compatibility is based on Android's current release documentation: https://developer.android.com/build/releases/about-agp

## Build locally

From the repository root, with Android SDK/NDK and Gradle 9.6.0 installed:

```sh
cd android
gradle :app:assembleDebug
```

Expected APK path:

`android/app/build/outputs/apk/debug/app-debug.apk`

## GitHub Actions

The workflow at `.github/workflows/android-smoke.yml` builds the debug APK on pushes and pull requests. The workflow result—not the presence of source files—is the build pass/fail evidence.

## What this proves

If the workflow succeeds and the APK launches on a device, it proves only:
- Gradle and Android plugin configuration resolve.
- The arm64 NDK compiler builds the native probe.
- JNI library loading works on that device.
- Basic NDK system calls used by the probe work.

## What this does not prove

- SDL3 integration.
- Vulkan instance/surface/swapchain.
- Android platform implementation for upstream `runtime/platform.h`.
- Guest memory reservation.
- ZA game-code generation/import.
- Game boot, rendering, audio, controls, performance or playability.

No proprietary game files or Nintendo assets are needed or included.
