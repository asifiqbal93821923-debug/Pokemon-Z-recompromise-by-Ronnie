# Android guest-page write-tracking capability probe

This diagnostic checks the optional Linux page-write tracking path used by upstream runtime/platform/linux.c. It does not load game code, change Android permissions, or alter the upstream runtime.

## Build #40 probe behavior

1. Query userfaultfd with UFFDIO_API(features=0) on a throwaway descriptor.
2. Record whether PAGEFAULT_FLAG_WP, WP_HUGETLBFS_SHMEM, WP_ASYNC, and WP_UNPOPULATED are advertised; negotiate advertised feature bits independently on fresh descriptors.
3. Test PAGEMAP_SCAN using the generic PAGE_IS_PRESENT category. This checks whether the ioctl itself is callable without assuming write-protect tracking is active.
4. Only if required WP features and the generic ioctl are available, register a one-page anonymous mapping with UFFDIO_REGISTER_MODE_WP, arm write protection, write the page, and query PAGE_IS_WRITTEN.
5. On AArch64, report soft-dirty as architecture-unsupported rather than running a misleading bit-55 test. Linux arm64 does not implement the soft-dirty PTE bit.
6. Release the one-page mapping and all descriptors.

## Interpreting results

- userfaultfd_open=PASS means the syscall is permitted in user-mode-only mode; it does not imply write-protect features are supported.
- UNSUPPORTED_NOT_ADVERTISED means the running kernel did not advertise that UFFD feature. An app permission dialog cannot enable it.
- pagemap_scan_generic=BLOCKED_BY_KERNEL_PERMISSION means the kernel denied the ioctl with EACCES or EPERM. This is not an Android runtime storage permission. Do not attempt to bypass SELinux or procfs restrictions.
- pagemap_scan_generic=UNSUPPORTED_OR_UAPI_MISMATCH means the kernel rejected the ioctl or its ABI.
- pagemap_scan_written=SKIPPED_MISSING_UFFD_WP_FEATURES is a dependency skip, not a failed written-page test.
- soft_dirty_result=UNSUPPORTED_ARCH_ARM64 is expected on AArch64; bit 55 is not a valid fallback there.
- pagemap_scan_written=PASS requires the complete userfaultfd WP registration/protection/write/scan sequence. Generic PAGEMAP_SCAN success alone is not written-page tracking.

## Runtime fallback

Upstream plat_written_pages() explicitly permits -1 when the host has no page-write tracking. The Vulkan renderer has conservative fallback behavior: it can mark/copy all pages in a queried range and disable GPU-overlap optimization when the write-watch is unavailable. This is slower, but safer than pretending a dirty-page tracker passed or using an unsafe process-wide signal handler.

A diagnostic PASS is only evidence for the tested API sequence. It does not prove the Android backend or game runtime is integrated.
