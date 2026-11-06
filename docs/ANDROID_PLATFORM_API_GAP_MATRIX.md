

## Build #47 owner device retest

The owner supplied Build #47 diagnostics from OnePlus CPH2943 / Android 16. The corrected probe ran on-device: userfaultfd open/query succeeded (mask `0x10ffe`), but the required `WP_ASYNC` and `WP_UNPOPULATED` features remain unadvertised. soft-dirty bit 55 was not observed across three pages. Generic PAGEMAP_SCAN ioctl returned EACCES, so procfs/SELinux policy blocked the test and vendor-backport support remains unknown. The safe runtime decision remains `plat_written_pages() == -1` on this device.

The same report reconfirmed exact 32-GiB guest-window reservation/protection, SDL3 event loop/multitouch, and Vulkan surface/device/queues/swapchain/first-frame presentation. Host and SDL Activity lifecycle logs include pause/stop/destroy and host Activity recreation/resume; no native fatal-error line appears, but the recreation cause is not established.

See `docs/ANDROID_PLATFORM_FUNCTION_CHECKLIST.md` for the function-by-function implementation map against the complete pinned upstream header. No production backend or game runtime is implemented yet.
