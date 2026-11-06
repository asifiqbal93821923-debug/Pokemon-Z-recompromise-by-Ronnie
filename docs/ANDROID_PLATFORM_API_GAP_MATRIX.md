# Android Platform API Gap Matrix

Audited against upstream Hexveil404/PokeZ-ARecomp, branch pokemon-za, pinned commit dfb47e9d8130d6ecde1c0afa1010fb1a102e50f6.

This document is an implementation plan, not evidence that an Android backend is implemented.

## Platform API audit

| Upstream API group | Android implementation decision | Current status |
|---|---|---|
| Guest address window | Dedicated Bionic implementation; exact address and collision handling | Exact 32-GiB reserve/release and one-page protection smoke test PASS |
| Written-page tracking | Prefer upstream UFFD async WP only if kernel supports it; otherwise return -1 and use renderer fallback | Build #39: no PAGEFAULT_FLAG_WP, WP_ASYNC, or WP_UNPOPULATED in advertised mask; AArch64 soft-dirty unsupported; generic PAGEMAP_SCAN permission check awaits Build #40 |
| Page state / memory usage | Return unknown where Android cannot report reliably | Not implemented |
| Monotonic timing | clock_gettime(CLOCK_MONOTONIC) and Android-compatible absolute sleep | Diagnostic clock read PASS; platform contract not implemented |
| Thread scheduling | Conservative pthread scheduling; gracefully handle Android policy denial | Not implemented |
| CPU/system info | Android properties and sysconf; no user-identifying data | Diagnostic CPU/ABI reporting available |
| Process lifecycle | Android Activity/SDL lifecycle; redesign constructor-time reservation and desktop re-exec | Not implemented |
| Child processes | Inventory helper tools; use in-process libraries or supported Android components | Not implemented |
| Filesystem | App-private paths; SAF URI/descriptor import bridge | Not implemented |
| Diagnostics | NDK-compatible crash/backtrace and Java UI bridge | Diagnostic log capture exists |
| Vulkan/SDL surface | Android surface lifecycle and swapchain recreation | Probe first-frame presentation PASS; recreation not tested |

## Write-tracking: root cause and safe fallback

Upstream Linux write tracking uses userfaultfd asynchronous write protection plus PAGEMAP_SCAN. The target device reports Android 16, kernel 6.1.157-android14-11-o-gdeed0ebc8d5d, ABI arm64-v8a.

Build #39 returned userfaultfd_open=PASS and an UFFDIO_API(features=0) query with mask 0x10ffe. That mask omits UFFD_FEATURE_PAGEFAULT_FLAG_WP, UFFD_FEATURE_WP_ASYNC, and UFFD_FEATURE_WP_UNPOPULATED; therefore the required userfaultfd write-protect path is unavailable on this tested kernel configuration.

The soft-dirty bit 55 is not a viable ARM64 fallback. Linux arm64 does not implement the soft-dirty PTE bit. Do not mark this as an application failure or keep retrying it.

The earlier PAGEMAP_SCAN check was not a valid PAGE_IS_WRITTEN test: it queried an ordinary page without UFFD WP registration/protection. It also received EACCES. Build #40 separates the generic PAGE_IS_PRESENT ioctl permission/capability test from the written-page test, which is attempted only after the complete UFFD WP prerequisites are available.

The app manifest has no dangerous runtime permission declarations and the Activities do not request permissions. EACCES is a kernel/procfs ioctl restriction, not a declined Android storage permission. Do not bypass SELinux, root the device, or claim an app permission can enable the ioctl.

### Safe runtime fallback

Upstream platform.h allows plat_written_pages() to return -1 when write tracking is unavailable. The Vulkan renderer has conservative fallback behavior: on unavailable page tracking it can mark/copy every page in a queried range and disable GPU-overlap optimization. This is a performance trade-off, not a false PASS. Do not add an unsafe process-wide SIGSEGV handler. Any future software-instrumented tracking must be validated against all generated guest memory stores, bulk writes, atomics, and helper writes before it can replace the fallback.

## Activity lifecycle review

The manifest contains a Java-only MainActivity and a separate-process SDL3 SDLProbeActivity. The owner-supplied Build #39 log shows create/start/resume/focus, then pause/focus-loss/stop, followed by start/resume/focus. This is consistent with normal background/foreground transitions, not a crash. The excerpt ends during touch input and does not include a final destroy/exit result. Repeated surface recreation and Vulkan swapchain rebuild remain untested.

## Remaining Android backend work

1. Compile a minimal Android implementation against the complete pinned upstream runtime/platform.h.
2. Use -1 for plat_written_pages() on this device; verify the renderer's conservative fallback path.
3. Add lifecycle-safe SDL/Vulkan surface recreation and repeated pause/resume tests.
4. Implement app-private data root and SAF import using a harmless test file.
5. Integrate upstream runtime sources only in a pinned, separately reviewed patch series.

No proprietary game data, keys, firmware, extracted assets, generated game code, or copyrighted material belongs in this repository.
