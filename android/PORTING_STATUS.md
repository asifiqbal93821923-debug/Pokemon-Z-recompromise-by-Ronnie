# Android Port Status

Last updated: 2026-11-06

## Overall state

**Android diagnostic shell is verified on the owner's phone through the first Vulkan frame and guest-window memory smoke test. The actual Pokémon Z-A recompilation runtime is not integrated.** The repository currently contains a research workspace and diagnostic application, not a playable game or game runtime.

## Verified target-device evidence

Device log supplied by the owner from Build #39, Android 16 / kernel `6.1.157-android14-11-o-gdeed0ebc8d5d`:

- Android native ABI: `arm64-v8a`.
- Device: OnePlus CPH2943; 8 online CPUs; 4096-byte pages.
- Display/window: 2400 × 1080.
- GPU: Mali-G615 MC2; vendor ID `0x13B5`; device ID `0xB8A31030`.
- Vulkan API: 1.3.247.
- Vulkan instance and Android surface: created.
- Graphics and present queues: available (queue family 0).
- Logical device: created.
- Swapchain: created, extent 2400 × 1080, format 37, six images.
- First frame: blue frame presented successfully.
- SDL3 event loop: ready.
- Touch and multi-touch: DOWN/UP events observed for simultaneous pointer IDs.
- Guest window: exact 32-GiB range at `0x7000000000` reserved, one-page commit/write/readback, read-only protection, `PROT_NONE`, and release all PASS.

Build #39 write-tracking evidence: `userfaultfd` opens and an `UFFDIO_API` query with no requested features succeeds (advertised mask `0x10ffe`). Neither `WP_ASYNC` nor `WP_UNPOPULATED` is advertised. Soft-dirty bit 55 was not set after the one-page clear/write probe. `PAGEMAP_SCAN` failed with `EACCES` (permission denied), so its kernel availability is still undetermined. The correct current Android backend behavior is to report written-page tracking as unsupported (`-1`), as allowed by upstream `platform.h`, until a safe alternative is proven.

These results validate diagnostic operations only. They do not validate upstream written-page tracking, guest fault handling, the upstream renderer, generated game code, audio, storage/import, or gameplay.

## Milestones

| Milestone | Status | Evidence / next action |
|---|---|---|
| Repository and Android port research plan | Done | Public repository, branch `main` |
| Upstream platform source audit | Done (source inspection) | `docs/ANDROID_PLATFORM_AUDIT.md` |
| Android JNI/NDK diagnostic | Device-tested PASS | Native probe; Build #24 report |
| SDL3 Android launch/event loop | Device-tested PASS | Build #26/#30/#39 logs |
| Touch and multi-touch delivery | Device-tested PASS | Build #26/#28/#30/#39 logs |
| Vulkan loader / instance / surface | Device-tested PASS | Build #30/#39 owner logs |
| Vulkan physical-device / queue selection | Device-tested PASS | Mali-G615 MC2, graphics/present queue family 0 |
| Vulkan logical device | Device-tested PASS | `ZA_VK_STAGE=logical_device_created` |
| Vulkan swapchain | Device-tested PASS | 2400×1080, six images |
| Vulkan first-frame presentation | Device-tested PASS | `ZA_VK_STAGE=first_frame_presented` |
| Android platform backend (`runtime/platform/android.c`) | Not started | Integrate only after source/dependency pin review |
| Upstream runtime entry point integration | Not started | Requires lifecycle-aware Android entry path |
| Exact 32-GiB guest-window reservation/protection | Device-tested PASS | Build #34 and reconfirmed Build #39 |
| Guest written-page tracking | Device-tested: unavailable on current kernel/policy | Build #39 isolated feature query; return `-1` for now |
| Android-compatible dependency build | Not started | SDL3 probe only; FFmpeg/shaderc/ImGui and runtime dependencies remain unverified |
| SAF user-file access/import | Not implemented | Add harmless test-file flow before game-data setup |
| Actual Z-A runtime startup | Not started | No game runtime or generated game code in this repository |
| Gameplay / performance | Not tested | No playable Android build exists |

## Build #39 — isolated kernel capability probe

- Commit: `d705dfb8b253500a3d4a6d7740ce6aaed3c79b91`
- Workflow run: [#40](https://github.com/asifiqbal93821923-debug/Pokemon-Z-recompromise-by-Ronnie/actions/runs/42180881533)
- CI: successful Android ARM64 debug build; APK archive and APK ZIP integrity verified.
- Device: owner supplied full diagnostics from OnePlus CPH2943 / Android 16 / kernel 6.1.157.
- `userfaultfd_open=PASS`; feature query succeeds with mask `0x10ffe`.
- `WP_ASYNC` and `WP_UNPOPULATED`: not advertised.
- Soft-dirty bit 55: not set in the test page after clear/write.
- `PAGEMAP_SCAN`: `EACCES` permission denied; not a definitive unsupported-kernel result.
- Vulkan first-frame, SDL event loop, touch/multitouch, and 32-GiB guest reservation/protection reconfirmed.
- Scope: diagnostics only; no actual runtime or gameplay.

## Build #38 — full diagnostics clipboard

- Commit: `e7e701c078cd403cfc292a4ef4eead90fa8de22c`
- Workflow run: [#38](https://github.com/asifiqbal93821923-debug/Pokemon-Z-recompromise-by-Ronnie/actions/runs/42179256198)
- CI: successful Android ARM64 debug build; APK archive and APK ZIP integrity verified.
- Added a MainActivity action to copy the complete app-private diagnostic log and device metadata.
- Owner verified the feature by supplying the copied diagnostics in Build #39 follow-up.

## Build #34 — guest address reservation

- Commit: `8683021e60b4793c026e581feb4b7a22306b5e30`
- Workflow run: [#34](https://github.com/asifiqbal93821923-debug/Pokemon-Z-recompromise-by-Ronnie/actions/runs/42175060697)
- CI: successful Android ARM64 debug build.
- Device: owner supplied `ZA_GUEST_MEM result=PASS`.
- Scope: exact 32-GiB virtual reservation and page-protection smoke test only.

## Build #30 — SDL/Vulkan frame probe

- Commit: `f23387203c81c6149b4de04642a36b9911d825a3`
- Workflow run: [#30](https://github.com/asifiqbal93821923-debug/Pokemon-Z-recompromise-by-Ronnie/actions/runs/42170871991)
- CI: successful Android ARM64 debug build.
- Device: owner supplied combined SDL + Vulkan log confirming the diagnostic stages above.
- Scope: diagnostic shell only, not Pokémon Z-A gameplay.

## Next engineering milestone

Do not label the current app as a port. The upstream audit is pinned to `dfb47e9d8130d6ecde1c0afa1010fb1a102e50f6` (branch `pokemon-za`). The next engineering step is a compile-tested Android platform backend skeleton against upstream `runtime/platform.h`, initially returning `-1` for `plat_written_pages()` on this device. Keep the diagnostic shell separate from the actual game runtime. The backend work should include:

1. An explicit Android NDK build target that leaves desktop targets unchanged.
2. A minimal Android implementation of the upstream platform contract.
3. Guest-window reservation/protection tests in the real SDL process.
4. Lifecycle-safe SDL/Vulkan surface recreation and repeated pause/resume testing.
5. App-private data paths and a SAF test-file import.
6. A dependency/license matrix for every native library.

No proprietary game data, keys, firmware, generated game code, or copyrighted assets may be committed to this repository.

## Status language

- Implemented = code exists.
- Build passed = CI/device build completed and artifact was checked.
- Device-tested = the owner exercised the feature on the stated device and supplied evidence.
- Runtime integrated = upstream runtime code is actually compiled and called by the Android app.
- Playable = actual in-game gameplay was observed. This status has not been reached.
