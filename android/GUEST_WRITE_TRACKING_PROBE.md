# Android guest-page write-tracking capability probe

This diagnostic checks the optional Linux page-write tracking path used by upstream runtime/platform/linux.c. It does not load game code, change Android permissions, or alter the upstream runtime.

## Build #44 probe behavior

1. Query userfaultfd with UFFDIO_API(features=0) on a throwaway descriptor.
2. Record whether PAGEFAULT_FLAG_WP, WP_HUGETLBFS_SHMEM, WP_ASYNC, and WP_UNPOPULATED are advertised; negotiate advertised feature bits independently on fresh descriptors.
3. Test PAGEMAP_SCAN using the generic PAGE_IS_PRESENT category. This checks whether the ioctl itself is callable without assuming write-protect tracking is active.
4. Only if required WP features and the generic ioctl are available, register a one-page anonymous mapping with UFFDIO_REGISTER_MODE_WP, arm write protection, write the page, and query PAGE_IS_WRITTEN.
5. Check soft-dirty across three pages and report NOT_OBSERVED separately. Stock upstream arm64 kernel 6.1 lacks the required soft-dirty architecture support; vendor backports remain unverified. Linux arm64 does not implement the soft-dirty PTE bit.
6. Release the one-page mapping and all descriptors.

## Interpreting results

- userfaultfd_open=PASS means the syscall is permitted in user-mode-only mode; it does not imply write-protect features are supported.
- UNSUPPORTED_NOT_ADVERTISED means the running kernel did not advertise that UFFD feature. An app permission dialog cannot enable it.
- pagemap_scan_generic=BLOCKED_BY_PROCFS_OR_SELINUX_POLICY means the ioctl was denied with EACCES or EPERM. This is not an Android runtime storage permission. On this device, the base kernel is 6.1 while upstream PAGEMAP_SCAN was introduced in 6.7; a vendor backport cannot be determined while access is blocked. Do not bypass SELinux.
- pagemap_scan_generic=UNSUPPORTED_OR_UAPI_MISMATCH means the kernel rejected the ioctl or its ABI.
- pagemap_scan_written=SKIPPED_MISSING_UFFD_WP_FEATURES is a dependency skip, not a failed written-page test.
- soft_dirty_result=UNSUPPORTED_ARCH_ARM64 is expected on AArch64; bit 55 is not a valid fallback there.
- pagemap_scan_written=PASS requires the complete userfaultfd WP registration/protection/write/scan sequence. Generic PAGEMAP_SCAN success alone is not written-page tracking.

## Runtime fallback

Upstream plat_written_pages() explicitly permits -1 when the host has no page-write tracking. In the pinned upstream Vulkan renderer (`runtime/nvn/vulkan/exec.c`), AUTO mode checks `pages_cpu_written()` and disables GPU overlap when tracking is unavailable; the normal same-frame fence wait is used instead. Do not force `LARECOMP_GPU_OVERLAP=1` or the GPU_OVERLAP compatibility setting ON on this Android path unless a separately validated tracking/hash-watch mode is implemented. This is safer than pretending a dirty-page tracker passed or using an unsafe process-wide signal handler.

A diagnostic PASS is only evidence for the tested API sequence. It does not prove the Android backend or game runtime is integrated.


## Build #47 — exact upstream feature prerequisite alignment

- Capability probe's write-protect readiness now matches the pinned upstream `runtime/platform/linux.c` implementation: `UFFD_FEATURE_WP_ASYNC` and `UFFD_FEATURE_WP_UNPOPULATED` are the required feature bits.
- `UFFD_FEATURE_PAGEFAULT_FLAG_WP` remains reported for diagnostics but is not treated as an extra prerequisite, because upstream `wt_init()` does not request it.
- This removes a possible false-negative classification. It does not enable missing kernel features or change the safe `plat_written_pages() = -1` fallback decision on the current device.


## Build #68 — Android safe fallback contract

The Android platform API probe calls `plat_written_pages()` and `plat_written_pages_peek()` on a committed guest page and requires both to return the upstream-defined `-1` sentinel. It logs `ZA_WRITE_TRACK fallback result=PASS mode=NO_TRACKER_RENDERER_AUTO_OVERLAP_OFF_REQUIRED` only when the sentinel contract is honored. The pinned upstream Vulkan renderer's AUTO path disables GPU overlap when page tracking is unavailable and returns to the normal same-frame fence wait. This does not claim dirty-page tracking or Android runtime integration works; the actual renderer setting and runtime call path still require an Android build/device test.
