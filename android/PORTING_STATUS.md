

### Build #47 — owner device retest (2026-11-06)

The owner supplied the full Build #47 `COPY FULL DIAGNOSTICS` report from OnePlus CPH2943 / Android 16 / arm64-v8a:

- Native JNI library loaded; 8 online CPUs; 4096-byte page size.
- SDL3 window/event loop and multitouch events passed.
- Vulkan Android surface, Mali-G615 MC2 (API 1.3.247), logical device, graphics/present queues, 2400×1080 six-image swapchain and first blue frame passed.
- Exact 32-GiB guest address reservation, one-page commit/write/readback, read-only protection, PROT_NONE and release passed.
- Corrected Build #47 feature gate was exercised. userfaultfd open and zero-feature query passed (mask `0x10ffe`), but `WP_ASYNC` and `WP_UNPOPULATED` remain unadvertised. Write tracking is therefore still unsupported on this device; use the upstream `-1` fallback.
- soft-dirty bit 55 was not observed on three pages; this is a device observation, not a universal AArch64 conclusion.
- PAGEMAP_SCAN generic ioctl returned EACCES / Permission denied. This is a procfs/SELinux policy block; support/backport status remains undetermined.
- MainActivity and SDL Activity lifecycle entries show orderly pause/stop/destroy and host Activity recreation/resume. No native fatal-error line appears in the supplied report; the reason for recreation is not proven.

This is device verification of the diagnostics only. It does not test the upstream runtime, production Android platform functions, generated game code or gameplay.

### Function-level platform API audit

Added `docs/ANDROID_PLATFORM_FUNCTION_CHECKLIST.md`, mapping every declaration in the pinned upstream `runtime/platform.h` to an Android implementation strategy and current evidence/status. This remains a plan; no production Android platform backend is claimed.


### Build #53 — Android platform API slice verified on device (2026-11-06)

The owner supplied the complete Build #53 diagnostics report from OnePlus CPH2943 / Android 16 / SDK 36 / arm64-v8a. The native Android platform slice was linked into the diagnostic JNI library for this isolated test (not into the upstream game runtime).

- `ZA_PLATFORM_API result=PASS`: exact 32-GiB guest reservation; commit/write/readback; read-only and PROT_NONE protection; one-page reset-to-zero; out-of-range protection rejection; monotonic clock; thread CPU clock; absolute monotonic sleep; CPU-affinity count (8); release.
- The same session reconfirmed the guest-memory probe PASS, Vulkan Android surface/device/queue/swapchain/first-frame PASS, SDL event loop and multitouch events.
- Write tracking remains unsupported on this device: userfaultfd opens, but required asynchronous write-protect features are not advertised; soft-dirty was not observed; PAGEMAP_SCAN generic ioctl is denied with EACCES. Keep `plat_written_pages()` and `plat_written_pages_peek()` returning -1.
- Host/SDL Activity lifecycle logs show pause/stop/destroy followed by host Activity recreation/resume. No native fatal-error line was supplied; the reason is not established.

**Scope:** this validates only the initial Android memory/time API slice in the diagnostic harness. It does not establish full upstream runtime integration, process/filesystem support, renderer compatibility, or playable gameplay.
