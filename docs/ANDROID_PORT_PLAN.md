# Android Port Plan

## Baseline

The upstream ZA Recomp project is a static recompilation-based native PC project. Its documentation describes Windows and experimental Linux support; Android is not listed as a supported platform. The runtime includes a Vulkan graphics implementation and SDL3 input, but that does not by itself make the application Android-compatible.

Primary references:
- https://github.com/Hexveil404/PokeZ-ARecomp/tree/pokemon-za
- https://github.com/Hexveil404/PokeZ-ARecomp/blob/pokemon-za/README.md
- https://github.com/Hexveil404/PokeZ-ARecomp/blob/pokemon-za/CONTRIBUTING.md
- https://github.com/Hexveil404/PokeZ-ARecomp/blob/pokemon-za/docs/LASRecomp-README.md

## Phases

### A — Upstream audit
- [ ] Confirm exact upstream branch and latest commit.
- [ ] Review root Makefile, platform abstraction, runtime entry point, SDL3 integration and Vulkan backend.
- [ ] Inventory all dependencies and their Android/ARM64 build support.
- [ ] Confirm license and preserve upstream notices.
- [ ] Identify desktop-only APIs and child-process assumptions.

### B — Android application shell
- [ ] Add Gradle project and Android application module.
- [ ] Configure Android NDK and CMake for arm64-v8a.
- [ ] Add SDL3 Android Activity integration.
- [ ] Implement app lifecycle pause/resume, surface recreation and back navigation.
- [ ] Set minimum/target SDK only after dependency requirements are known.

### C — Android platform layer
- [ ] Implement a dedicated Android platform backend; do not treat Android as interchangeable with desktop Linux.
- [ ] Audit guest virtual-memory reservation, commit/protection and page tracking.
- [ ] Implement monotonic timing, sleep, CPU/thread information and supported priority handling.
- [ ] Route writable data to app-private storage.
- [ ] Adapt file operations, diagnostics, logging and crash reporting.
- [ ] Replace or redesign child-process/helper workflows where Android restrictions require it.

### D — Graphics and input
- [ ] Create Android Vulkan surface and lifecycle-safe swapchain.
- [ ] Check physical-device Vulkan extensions/features and memory limits at runtime.
- [ ] Verify shader/compiler dependencies on Android ARM64.
- [ ] Integrate SDL3 touch and multi-touch with the game input layer.
- [ ] Implement left-side movement drag zone, right-side camera drag and contextual action controls only after the game's real input mapping is verified.
- [ ] Test simultaneous movement, camera and action input.

### E — User-owned game data
- [ ] Start with Android Storage Access Framework selection of user-owned extracted game data.
- [ ] Handle content-provider URIs that are seekable versus pipe/non-seekable.
- [ ] If importing to app-private storage is needed, check free space, show progress and support cancellation/retry.
- [ ] Keep game data, keys, firmware, generated game code and saves out of Git.
- [ ] Consider Android-native package import only after the runtime and extracted-data path work.

### F — Build and device verification
- [ ] Build arm64-v8a debug APK.
- [ ] Install and launch on the target Android device.
- [ ] Verify lifecycle, storage permissions, logging and Vulkan initialization.
- [ ] Run a minimal runtime test before attempting full game setup.
- [ ] Test game setup and gameplay only with the user's own legally obtained files.
- [ ] Record crashes, graphics issues, thermals, memory use and frame pacing.
- [ ] Do not label the port playable until actual in-game testing succeeds.

## Key technical risks

1. Desktop Linux system calls and facilities may be unavailable or restricted on Android.
2. Guest-memory reservation/protection semantics may not match Android's process environment.
3. SDL3 and Vulkan require Android-specific Activity/surface/lifecycle integration.
4. Recompiler tooling and compiler helpers may rely on desktop Python, .NET, Zig or subprocess workflows.
5. ARM64 host code generation and all native dependencies need Android NDK compatibility.
6. A phone's Vulkan driver, GPU memory and thermal envelope may prevent acceptable performance.

## First implementation milestone

The first meaningful milestone is **a minimal Android arm64 native shell that builds, launches, logs device/Vulkan capabilities and handles pause/resume**. This is deliberately before game-code generation or full gameplay.
