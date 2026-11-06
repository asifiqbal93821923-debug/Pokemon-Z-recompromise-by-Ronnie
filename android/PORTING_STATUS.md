# Android Port Status

Last updated: 2026-11-06

## Overall state

**Android diagnostic shell is verified on the owner's phone through the first Vulkan frame. The actual Pokémon Z-A recompilation runtime is not integrated.** The repository currently contains a research workspace and diagnostic application, not a playable game or game runtime.

## Verified target-device evidence

Device log supplied by the owner from Build #30:

- Android native ABI: `arm64-v8a` (confirmed by prior JNI probe).
- Display/window: 2400 × 1080.
- GPU: Mali-G615 MC2; vendor ID `0x13B5`; device ID `0xB8A31030`.
- Vulkan API: 1.3.247.
- Vulkan instance and Android surface: created.
- Graphics and present queues: available (queue family 0).
- Logical device: created.
- Swapchain: created, extent 2400 × 1080, format 37, six images.
- First frame: blue frame presented successfully.
- SDL3 event loop: ready.
- Touch and multi-touch: DOWN/UP events observed for multiple pointer IDs.

Build #34 additionally confirms exact 32-GiB guest-window reservation at `0x7000000000`, one-page RW commit/write/readback, read-only protection, `PROT_NONE`, and full `munmap` release. This validates only the tested mmap/mprotect sequence; it does not validate upstream written-page tracking, fault handling, guest memory semantics, the upstream renderer, generated game code, audio, storage/import, or gameplay.

## Milestones

| Milestone | Status | Evidence / next action |
|---|---|---|
| Repository and Android port research plan | Done | Public repository, branch `main` |
| Upstream platform source audit | Done (source inspection) | `docs/ANDROID_PLATFORM_AUDIT.md` |
| Android JNI/NDK diagnostic | Device-tested PASS | Prior native probe and Build #24 device report |
| SDL3 Android launch/event loop | Device-tested PASS | Build #26/#30; SDL event loop ready |
| Touch and multi-touch delivery | Device-tested PASS | Build #26/#28/#30 logs |
| Vulkan loader / instance / surface | Device-tested PASS | Build #30 owner log |
| Vulkan physical-device / queue selection | Device-tested PASS | Mali-G615 MC2, graphics/present queue family 0 |
| Vulkan logical device | Device-tested PASS | `ZA_VK_STAGE=logical_device_created` |
| Vulkan swapchain | Device-tested PASS | `ZA_VK_STAGE=swapchain_created`; 2400×1080, six images |
| Vulkan first-frame presentation | Device-tested PASS | `ZA_VK_STAGE=first_frame_presented` |
| Android platform backend (`runtime/platform/android.c`) | Not started | Upstream runtime source is not present in this workspace; integrate only after a license/dependency/source pin review |
| Upstream runtime entry point integration | Not started | Requires lifecycle-aware Android entry path |
| Exact 32-GiB guest-window reservation/protection | Device-tested PASS | Build #34: exact address, one-page commit/write/readback, read-only/`PROT_NONE`, release |
| Guest written-page tracking / fault handling | Not tested | Probe Android `userfaultfd`/`PAGEMAP_SCAN` availability and permissions; design a safe fallback if unavailable |
| Android-compatible dependency build | Not started | SDL3 probe only; FFmpeg/shaderc/ImGui and runtime dependencies remain unverified |
| SAF user-file access/import | Not implemented | Add a harmless test-file flow before game-data setup |
| Actual Z-A runtime startup | Not started | No game runtime or generated game code in this repository |
| Gameplay / performance | Not tested | No playable Android build exists |

## Build #34 — guest address reservation

- Commit: `8683021e60b4793c026e581feb4b7a22306b5e30`
- Workflow run: [#34](https://github.com/asifiqbal93821923-debug/Pokemon-Z-recompromise-by-Ronnie/actions/runs/42175060697)
- CI: successful Android ARM64 debug build.
- Device: owner supplied the combined log with `ZA_GUEST_MEM result=PASS`.
- Scope: exact 32-GiB virtual reservation and page protection smoke test only; not upstream fault tracking or runtime integration.

## Build #30

- Commit: `f23387203c81c6149b4de04642a36b9911d825a3`
- Workflow run: [#30](https://github.com/asifiqbal93821923-debug/Pokemon-Z-recompromise-by-Ronnie/actions/runs/42170871991)
- CI: successful Android ARM64 debug build.
- Device: owner supplied a combined SDL + Vulkan log confirming the diagnostic stages above.
- Scope: diagnostic shell only, not Pokémon Z-A gameplay.

## Next engineering milestone

Do not label the current app as a port. The upstream audit is now pinned to `dfb47e9d8130d6ecde1c0afa1010fb1a102e50f6` (branch `pokemon-za`). Its `runtime/rt.h` defines the non-Apple guest window as `0x7000000000`–`0x7800000000`, a 32-GiB reservation. The exact-range reservation/protection smoke test passed on the owner's phone in Build #34. The next gate is to test the Linux-specific written-page tracking/fault mechanisms used by upstream and establish a compatible Android implementation before importing runtime code. Then create a separate Android runtime integration branch/patch series with:

1. An explicit Android NDK build target that leaves desktop targets unchanged.
2. A minimal, compile-tested Android implementation of the upstream platform contract.
3. A guest-memory reservation/protection test before attempting runtime startup.
4. A lifecycle-safe SDL/Vulkan host loop with swapchain recreation on surface changes.
5. App-private writable paths and a SAF test-file reader.
6. A dependency/license matrix for every native library.

No proprietary game data, keys, firmware, generated game code, or copyrighted assets may be committed to this repository.

## Status language

- Implemented = code exists.
- Build passed = CI/device build completed and artifact was checked.
- Device-tested = the owner exercised the feature on the stated device and supplied evidence.
- Runtime integrated = upstream runtime code is actually compiled and called by the Android app.
- Playable = actual in-game gameplay was observed. This status has not been reached.
