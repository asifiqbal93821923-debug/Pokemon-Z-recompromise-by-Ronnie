# Android Platform Layer — Implementation Contract

This file translates the upstream `runtime/platform.h` interface into Android implementation requirements. It is a design checklist, not an implementation.

## Runtime contract

| API area | Android approach | Initial support policy |
|---|---|---|
| Guest address space | Use NDK-supported `mmap`/ `mprotect` only after testing exact guest range and collision behavior | Required; fail safely if exact range cannot be reserved |
| Commit/reset | Preserve upstream guest memory semantics; verify page alignment and Android page size | Required |
| Written-page tracking | Do not assume `userfaultfd` or `PAGEMAP_SCAN` availability | Return `-1` / unsupported initially |
| Memory usage/page state | Use supported process metrics only | Unknown/false where unavailable |
| Monotonic time | `clock_gettime(CLOCK_MONOTONIC)` | Required |
| Sleep | Android/Bionic-supported clock sleep; handle interruption | Required |
| CPU count | Use supported online/affinity information; report unknown if restricted | Best effort |
| Thread priority | Use Android-supported pthread/scheduling interfaces; do not request privileged real-time policy | Best effort, safe defaults |
| App/process info | Android package/app metadata and NDK process information | Required for diagnostics |
| Executable/resource paths | Android app native library/resource locations | Required; never assume executable directory is writable |
| Data root | App-private files/cache directories | Required |
| Child processes | Avoid desktop helper process model; move supported helpers in-process or redesign workflow | Not supported until explicitly implemented |
| File operations | POSIX/Bionic operations inside app-private storage; SAF for user documents | Required |
| Backtrace/crash | Android logcat + supported native crash/backtrace mechanism | Best effort |
| Fatal notice | Surface errors through Android UI/Activity bridge, not desktop message boxes | Required for user-facing errors |

## Build integration requirements

- Add an explicit `android` platform branch in the Makefile or move the Android target to a dedicated CMake build.
- Select `arm64-v8a` deliberately; do not infer Android ABI from host `uname`.
- Use Android NDK toolchain and Android-compatible dependency builds.
- Add Android SDL3 Activity integration.
- Build Vulkan backend against Android Vulkan headers/loader and create an Android surface/swapchain.
- Do not include Windows-only libraries/flags in Android builds.
- Keep game-specific generated code and all user game data outside the repository.

## Lifecycle requirements

- Pause/resume must stop or safely suspend rendering/audio as appropriate.
- Surface loss/recreation must rebuild swapchain resources without losing guest state unexpectedly.
- Activity recreation and process death must not corrupt saves or data imports.
- Back navigation must be handled deliberately.
- Touch input must support simultaneous pointers and route into the game's SDL3 input abstraction.

## Acceptance gates

1. NDK compiles a minimal JNI/native library.
2. SDL3 Activity launches and logs Android version/ABI.
3. Pause/resume and surface recreation work.
4. Android platform API subset compiles and unit/smoke tests pass.
5. Vulkan instance/device/surface/swapchain initialize on the target phone.
6. SAF selection can read a test file safely; no proprietary game file is needed for this test.
7. Only then proceed to runtime/game-data integration.
