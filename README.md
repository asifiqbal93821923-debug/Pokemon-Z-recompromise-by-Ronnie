# Pokémon Legends: Z-A — Android Port Research Workspace

This repository tracks an experimental effort to investigate an **Android-native port** of Pokémon Legends: Z-A based on the open-source ZA Recomp project.

## Important status

**Research/scaffolding only. No Android game build or APK exists yet.** The upstream project currently documents Windows as its primary target and Linux as experimental; Android is not a supported target. An Android port requires substantial platform, build, graphics, lifecycle, storage and input work.

This repository is a planning and porting workspace. It does not currently contain the upstream game engine, Nintendo code/assets, extracted game data, generated game code, keys, firmware, or a playable game.

## Upstream source

- Project: [Hexveil404/PokeZ-ARecomp](https://github.com/Hexveil404/PokeZ-ARecomp)
- Development branch referenced for research: `pokemon-za`
- Upstream documentation and license must be checked again before importing or adapting any source.
- This workspace does not copy upstream source wholesale. Any future source integration must preserve notices, comply with each dependency's license, and keep proprietary game data out of Git.

## Planned work

1. Verify upstream architecture, branch, license and build dependencies.
2. Add an Android application shell and Android NDK build integration.
3. Implement an Android-specific platform layer rather than assuming desktop Linux APIs work on Android.
4. Integrate SDL3 Android lifecycle, Vulkan surface/swapchain and multi-touch input.
5. Add safe access to user-selected local game files through Android's Storage Access Framework.
6. Build and test on a real Android device; report failures honestly before claiming support.

See [Android port plan](docs/ANDROID_PORT_PLAN.md), [source index](docs/SOURCE_INDEX.md), and [porting status](android/PORTING_STATUS.md).

## Legal and safety boundary

Only use game files that the user is legally entitled to use. Do not commit or upload NSP/XCI packages, prod.keys, firmware, extracted Nintendo files, generated game code, copyrighted assets, fonts, saves, or other proprietary material. Keep user game data on the user's device and follow applicable law and upstream project instructions.

## Device target

Initial target device reported by the project owner: Android 16 phone, 8 GB RAM, 128 GB storage. Exact SoC/GPU and Vulkan capabilities have **not** yet been verified. No performance or compatibility claim is made.

## Contributions and verification

Every implementation step must record the exact files changed, build command, build result, device/OS/GPU tested, and any known limitations. A successful source edit is not a successful build; a successful build is not proof that the game runs.
