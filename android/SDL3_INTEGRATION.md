# SDL3 Android Integration Smoke Test

This milestone replaces the plain JNI text Activity with SDL3's supported Android Activity glue and a small native SDL event/render loop.

## Pinned dependency

- SDL upstream: https://github.com/libsdl-org/SDL
- Tag: `release-3.4.18`
- CI checks out this exact tag into `android/app/src/main/cpp/SDL`.
- The upstream SDL Android Java source tree is added to the app's Java source set.
- CMake builds SDL from source for `arm64-v8a` and links the app's native `main` shared library against `SDL3::SDL3`.

The SDL Android documentation describes the Java Activity/JNI bridge, recommends subclassing `SDLActivity`, and documents building SDL as part of a CMake project. See https://wiki.libsdl.org/SDL3/README-android and https://github.com/libsdl-org/SDL/blob/main/docs/README-android.md.

## What this probe exercises

- SDL3 Android Activity startup.
- Native SDL video and event initialization.
- Fullscreen SDL window and renderer creation.
- SDL finger down/up/motion event delivery.
- Multi-touch event identity logging.
- SDL renderer lifecycle and orderly shutdown.

Touching the screen changes the test background; it is only a diagnostic visual, not the planned game control overlay.

## What it does not do

- It does not link the ZA Recomp runtime.
- It does not create an Android Vulkan surface or Vulkan swapchain.
- It does not implement the final movement/camera/action controls.
- It does not import, decrypt, or load game data.
- It is not a playable Pokémon Z-A build.

## Build

CI uses the pinned SDL release and runs:

```sh
gradle --no-daemon -p android :app:assembleDebug
```

Artifact name: `pokemon-za-android-sdl3-smoke-arm64-debug`.

## Next gate

Only after this build passes should we add a Vulkan-specific SDL window/surface test, then begin Android runtime platform work. The actual device GPU and Vulkan extension support must be measured on the target phone; no GPU assumptions are made.
