# Android Port Status

Last updated: 2026-11-06

## Overall state

**Android diagnostic shell is verified on the owner's phone through the first Vulkan frame and guest-window memory smoke test. The actual Pokémon Z-A recompilation runtime is not integrated.** This repository is a research workspace and diagnostic application, not a playable game or game runtime.

## Verified target-device evidence

Device log supplied by the owner from Build #39, Android 16 / kernel 6.1.157-android14-11-o-gdeed0ebc8d5d:

- Android native ABI: arm64-v8a; OnePlus CPH2943; 8 online CPUs; 4096-byte pages.
- Display/window: 2400 × 1080; GPU Mali-G615 MC2; Vulkan API 1.3.247.
- Vulkan instance, Android surface, logical device, graphics/present queues and six-image 2400×1080 swapchain created; first blue frame presented.
- SDL3 event loop ready; simultaneous touch pointer IDs delivered.
- Exact 32-GiB guest window at 0x7000000000 reserved; one-page commit/write/readback, read-only protection, PROT_NONE, and release passed.

### Write-tracking diagnosis from Build #39

- userfaultfd() opens in UFFD_USER_MODE_ONLY mode and the zero-feature UFFDIO_API query succeeds (mask 0x10ffe).
- The advertised mask does not contain UFFD_FEATURE_PAGEFAULT_FLAG_WP, WP_ASYNC, or WP_UNPOPULATED. Therefore the kernel does not expose the userfaultfd write-protect path required by upstream async dirty-page tracking.
- The bit-55 soft-dirty test returned clear. Do not generalize this to all AArch64 kernels: upstream arm64 kernel 6.1 lacks the required ARCH_HAS_SOFT_DIRTY implementation; a vendor backport is possible but has not been demonstrated on this phone. The result is NOT_OBSERVED on this device, not a proven universal ARM64 limitation.
- PAGEMAP_SCAN was introduced upstream in Linux 6.7; this phone reports kernel 6.1.157. The original ioctl returned EACCES, so Android procfs/SELinux policy blocked the app before kernel ioctl support could be established. The original PAGE_IS_WRITTEN query also lacked UFFD write-protect registration, so it was not a valid written-page test. Build #44 separates generic PAGE_IS_PRESENT probing from written-page tracking and labels permission denial separately. A vendor backport remains unverified.
- The app manifest requests no dangerous Android runtime permissions, and the Java Activities do not request any. The EACCES is a kernel/procfs ioctl denial, not a user declining Photos/Files permission. There is no safe in-app permission prompt that can grant it.
- Runtime policy: plat_written_pages() should return -1 on this device until a supported mechanism is proven. Upstream explicitly allows this. Its Vulkan renderer has conservative fallback paths that copy all pages in the queried range and disable GPU-overlap optimization when tracking is unavailable; this can cost performance but avoids false dirty-page reports.

### Android Activity/lifecycle review

The manifest declares no `<uses-permission>` entries, so there is no storage/media runtime permission for the user to have declined. It declares two Activities:
- MainActivity: Java-only host/diagnostics screen in the main app process. It launches the SDL probe and reads/copies the app-private log.
- SDLProbeActivity: SDL3 diagnostic Activity in the separate :sdlprobe process. It records create/start/resume/pause/stop/destroy and window-focus changes.

The owner's Build #39 log shows SDL Activity create → start → resume → focus true, then pause → focus false → stop, followed by start → resume → focus true. This is a normal background/foreground lifecycle sequence, not evidence of a crash. The pasted report ends during subsequent touch events, so it does not prove what happened after that point. No ACTIVITY_ON_DESTROY appears in the supplied excerpt. Build #45 adds separate MainActivity lifecycle breadcrumbs so both declared Activities can be reviewed.

## Milestones

| Milestone | Status | Evidence / next action |
|---|---|---|
| Repository and Android port research plan | Done | Public repository, branch main |
| Upstream platform source audit | Done (source inspection) | docs/ANDROID_PLATFORM_AUDIT.md |
| Android JNI/NDK diagnostic | Device-tested PASS | Native probe; Build #24 report |
| SDL3 Android launch/event loop | Device-tested PASS | Build #26/#30/#39 logs |
| Touch and multi-touch delivery | Device-tested PASS | Build #26/#28/#30/#39 logs |
| Vulkan first-frame presentation | Device-tested PASS | Build #30/#39 owner logs |
| Exact 32-GiB guest-window reservation/protection | Device-tested PASS | Build #34/#39 logs |
| Kernel write-tracking capability | Required UFFD WP features absent; soft-dirty not observed; PAGEMAP_SCAN blocked by policy / base kernel 6.1 predates upstream 6.7 | Use safe upstream fallback; do not force kernel privileges |
| Android platform backend | Not started | Compile a minimal Android implementation against pinned upstream platform.h |
| Upstream runtime integration | Not started | Requires lifecycle-aware Android entry path |
| Android-compatible dependencies | Not started | SDL3 probe only; FFmpeg/shaderc/ImGui/runtime dependencies unverified |
| SAF file import | Not implemented | Add harmless test-file flow |
| Actual Z-A runtime startup | Not started | No game runtime/generated game code in this repository |
| Gameplay / performance | Not tested | No playable Android build exists |

## Build #44 — kernel/policy-aware capability diagnostics

- Probe source commit: `a90a4e9b5404c03504c224f525deef2027acca5a`.
- Logs the exact kernel release and identifies upstream PAGEMAP_SCAN as a Linux 6.7+ interface; a 6.1 vendor backport is not assumed.
- Separates EACCES/EPERM policy denial, ENOTTY unsupported ioctl, and EINVAL ABI mismatch.
- Tests generic PAGE_IS_PRESENT; skips PAGE_IS_WRITTEN unless required UFFD WP features are advertised.
- Repeats soft-dirty checks across three pages and reports NOT_OBSERVED without falsely claiming universal AArch64 behavior.
- Scope remains diagnostic only; no runtime/gameplay.

## Build #39 — original isolated kernel capability probe

- Commit: d705dfb8b253500a3d4a6d7740ce6aaed3c79b91
- Workflow: [#40](https://github.com/asifiqbal93821923-debug/Pokemon-Z-recompromise-by-Ronnie/actions/runs/42180881533)
- CI: successful Android ARM64 debug build; APK ZIP integrity verified.
- Owner supplied full OnePlus CPH2943 / Android 16 diagnostics.
- Vulkan first-frame, SDL event loop, touch/multitouch and guest-window reservation/protection reconfirmed.
- Scope: diagnostic shell only; no actual runtime or gameplay.

## Next engineering milestone

Keep the diagnostic shell separate from the game runtime. The next engineering step is a compile-tested Android platform backend skeleton against upstream runtime/platform.h, with safe unsupported results where Android lacks an equivalent. Add lifecycle-safe surface recreation, app-private paths and SAF import before attempting runtime integration.

No proprietary game data, keys, firmware, generated game code, or copyrighted assets may be committed to this repository.

## Status language

- Implemented = code exists.
- Build passed = CI/device build completed and artifact was checked.
- Device-tested = the owner exercised the feature on the stated device and supplied evidence.
- Runtime integrated = upstream runtime code is actually compiled and called by the Android app.
- Playable = actual in-game gameplay was observed. This status has not been reached.
