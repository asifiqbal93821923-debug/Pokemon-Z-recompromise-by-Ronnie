#include <SDL3/SDL.h>

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <linux/userfaultfd.h>

#include "guest_write_tracking_probe.h"

#ifndef UFFD_USER_MODE_ONLY
#define UFFD_USER_MODE_ONLY 1
#endif
#ifndef UFFD_FEATURE_PAGEFAULT_FLAG_WP
#define UFFD_FEATURE_PAGEFAULT_FLAG_WP (1ULL << 0)
#endif
#ifndef UFFD_FEATURE_WP_HUGETLBFS_SHMEM
#define UFFD_FEATURE_WP_HUGETLBFS_SHMEM (1ULL << 12)
#endif
#ifndef UFFD_FEATURE_WP_UNPOPULATED
#define UFFD_FEATURE_WP_UNPOPULATED (1ULL << 13)
#endif
#ifndef UFFD_FEATURE_WP_ASYNC
#define UFFD_FEATURE_WP_ASYNC (1ULL << 15)
#endif

/* Linux PAGEMAP_SCAN UAPI: struct pm_scan_arg and struct page_region. */
typedef struct {
    uint64_t start;
    uint64_t end;
    uint64_t categories;
} ZaWtRegion;

typedef struct {
    uint64_t size, flags, start, end, walk_end, vec, vec_len, max_pages;
    uint64_t category_inverted, category_mask, category_anyof_mask, return_mask;
} ZaWtScan;

#define ZA_PAGEMAP_SCAN _IOWR('f', 16, ZaWtScan)
#define ZA_PAGE_IS_PRESENT (1ULL << 3)
#define ZA_PAGE_IS_WRITTEN (1ULL << 1)
#define ZA_PAGEMAP_PRESENT_BIT (1ULL << 63)
#define ZA_SOFT_DIRTY_BIT (1ULL << 55)

static void log_errno(const char *stage)
{
    const int saved_errno = errno;
    SDL_Log("ZA_WRITE_TRACK stage=%s result=FAIL errno=%d message=%s",
            stage, saved_errno, strerror(saved_errno));
}

static bool kernel_version(int *major, int *minor)
{
    struct utsname info;
    if (uname(&info) != 0) {
        log_errno("uname");
        return false;
    }
    *major = 0;
    *minor = 0;
    if (sscanf(info.release, "%d.%d", major, minor) != 2) {
        SDL_Log("ZA_WRITE_TRACK kernel_release=%s parse=UNKNOWN", info.release);
        return false;
    }
    SDL_Log("ZA_WRITE_TRACK kernel_release=%s parsed=%d.%d",
            info.release, *major, *minor);
    return true;
}

#if defined(SYS_userfaultfd)
static int open_userfaultfd(void)
{
    return (int)syscall(SYS_userfaultfd,
                        O_CLOEXEC | O_NONBLOCK | UFFD_USER_MODE_ONLY);
}
#endif

static uint64_t probe_userfaultfd_features(void)
{
#if !defined(SYS_userfaultfd)
    SDL_Log("ZA_WRITE_TRACK userfaultfd=UNAVAILABLE reason=SYS_userfaultfd_not_defined");
    return 0;
#else
    int fd = open_userfaultfd();
    if (fd < 0) {
        log_errno("userfaultfd_open");
        SDL_Log("ZA_WRITE_TRACK userfaultfd=UNAVAILABLE");
        return 0;
    }

    SDL_Log("ZA_WRITE_TRACK userfaultfd_open=PASS fd=%d", fd);
    struct uffdio_api api = {.api = UFFD_API, .features = 0};
    if (ioctl(fd, UFFDIO_API, &api) < 0) {
        log_errno("uffdio_api_query_features_zero");
        close(fd);
        return 0;
    }

    const uint64_t available = api.features;
    SDL_Log("ZA_WRITE_TRACK uffdio_api_query=PASS available_features=0x%llx generic_ioctls=0x%llx",
            (unsigned long long)available, (unsigned long long)api.ioctls);
    close(fd);

    const struct {
        const char *name;
        uint64_t bit;
    } features[] = {
        {"PAGEFAULT_FLAG_WP", UFFD_FEATURE_PAGEFAULT_FLAG_WP},
        {"WP_HUGETLBFS_SHMEM", UFFD_FEATURE_WP_HUGETLBFS_SHMEM},
        {"WP_UNPOPULATED", UFFD_FEATURE_WP_UNPOPULATED},
        {"WP_ASYNC", UFFD_FEATURE_WP_ASYNC}
    };
    for (size_t i = 0; i < sizeof(features) / sizeof(features[0]); ++i) {
        SDL_Log("ZA_WRITE_TRACK feature=%s advertised=%s",
                features[i].name,
                (available & features[i].bit) ? "yes" : "no");
    }
    return available;
#endif
}

static void probe_soft_dirty(size_t page_size)
{
#if defined(__aarch64__)
    SDL_Log("ZA_WRITE_TRACK soft_dirty_arch=arm64 kernel_6_1_stock_support=not_expected vendor_backport=unknown");
#endif
    void *pages = mmap(NULL, page_size * 3, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (pages == MAP_FAILED) {
        log_errno("soft_dirty_test_mmap");
        return;
    }

    volatile uint32_t *words = (volatile uint32_t *)pages;
    for (size_t i = 0; i < 3; ++i)
        words[(i * page_size) / sizeof(uint32_t)] = UINT32_C(0x5A413100) + (uint32_t)i;

    int clear_refs = open("/proc/self/clear_refs", O_WRONLY | O_CLOEXEC);
    if (clear_refs < 0) {
        log_errno("soft_dirty_open_clear_refs");
        (void)munmap(pages, page_size * 3);
        return;
    }

    const char command = '4';
    const ssize_t cleared = write(clear_refs, &command, 1);
    close(clear_refs);
    if (cleared != 1) {
        if (cleared >= 0) errno = EIO;
        log_errno("soft_dirty_clear_refs_write");
        (void)munmap(pages, page_size * 3);
        return;
    }

    /* Volatile stores occur strictly after clear_refs has returned. */
    for (size_t i = 0; i < 3; ++i)
        words[(i * page_size) / sizeof(uint32_t)] = UINT32_C(0x5A413200) + (uint32_t)i;

    int pagemap = open("/proc/self/pagemap", O_RDONLY | O_CLOEXEC);
    if (pagemap < 0) {
        log_errno("soft_dirty_open_pagemap");
        (void)munmap(pages, page_size * 3);
        return;
    }

    unsigned int present_count = 0;
    unsigned int dirty_count = 0;
    for (size_t i = 0; i < 3; ++i) {
        const uint64_t address = (uint64_t)(uintptr_t)pages + i * (uint64_t)page_size;
        const uint64_t page_index = address / (uint64_t)page_size;
        const off_t offset = (off_t)(page_index * sizeof(uint64_t));
        uint64_t entry = 0;
        const ssize_t count = pread(pagemap, &entry, sizeof(entry), offset);
        if (count != (ssize_t)sizeof(entry)) {
            if (count >= 0) errno = EIO;
            log_errno("soft_dirty_read_pagemap_entry");
            continue;
        }
        if (entry & ZA_PAGEMAP_PRESENT_BIT) ++present_count;
        if (entry & ZA_SOFT_DIRTY_BIT) ++dirty_count;
        SDL_Log("ZA_WRITE_TRACK soft_dirty_page=%zu present=%s bit55=%s entry=0x%llx",
                i, (entry & ZA_PAGEMAP_PRESENT_BIT) ? "yes" : "no",
                (entry & ZA_SOFT_DIRTY_BIT) ? "SET" : "NOT_SET",
                (unsigned long long)entry);
    }
    close(pagemap);
    SDL_Log("ZA_WRITE_TRACK soft_dirty_summary present=%u/3 dirty=%u/3 result=%s",
            present_count, dirty_count,
            dirty_count ? "OBSERVED" : "NOT_OBSERVED");
    (void)munmap(pages, page_size * 3);
}

static bool probe_pagemap_scan_generic(size_t page_size)
{
    void *page = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED) {
        log_errno("pagemap_scan_test_mmap");
        return false;
    }
    *(volatile uint32_t *)page = UINT32_C(0x5A413303);

    int fd = open("/proc/self/pagemap", O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        log_errno("pagemap_scan_open_pagemap");
        (void)munmap(page, page_size);
        return false;
    }

    ZaWtRegion regions[4] = {{0}};
    ZaWtScan scan = {
        .size = sizeof(scan),
        .flags = 0,
        .start = (uint64_t)(uintptr_t)page,
        .end = (uint64_t)(uintptr_t)page + (uint64_t)page_size,
        .walk_end = 0,
        .vec = (uint64_t)(uintptr_t)regions,
        .vec_len = 4,
        .max_pages = 0,
        .category_inverted = 0,
        .category_mask = ZA_PAGE_IS_PRESENT,
        .category_anyof_mask = 0,
        .return_mask = ZA_PAGE_IS_PRESENT
    };

    const long found = ioctl(fd, ZA_PAGEMAP_SCAN, &scan);
    if (found < 0) {
        const int err = errno;
        log_errno("pagemap_scan_generic_ioctl");
        if (err == EACCES || err == EPERM) {
            SDL_Log("ZA_WRITE_TRACK pagemap_scan_generic=BLOCKED_BY_PROCFS_OR_SELINUX_POLICY");
        } else if (err == ENOTTY) {
            SDL_Log("ZA_WRITE_TRACK pagemap_scan_generic=UNSUPPORTED_IOCTL");
        } else if (err == EINVAL) {
            SDL_Log("ZA_WRITE_TRACK pagemap_scan_generic=UAPI_OR_KERNEL_MISMATCH");
        } else {
            SDL_Log("ZA_WRITE_TRACK pagemap_scan_generic=ERROR errno=%d", err);
        }
        close(fd);
        (void)munmap(page, page_size);
        return false;
    }

    bool present = false;
    for (long i = 0; i < found && i < 4; ++i) {
        if (regions[i].start <= (uint64_t)(uintptr_t)page &&
            regions[i].end >= (uint64_t)(uintptr_t)page + (uint64_t)page_size &&
            (regions[i].categories & ZA_PAGE_IS_PRESENT)) {
            present = true;
            break;
        }
    }
    SDL_Log("ZA_WRITE_TRACK pagemap_scan_generic=CALL_PASS regions=%ld walk_end=0x%llx test_page_present=%s",
            found, (unsigned long long)scan.walk_end, present ? "yes" : "no");
    close(fd);
    (void)munmap(page, page_size);
    return true;
}

void za_run_guest_write_tracking_probe(void)
{
    SDL_Log("ZA_WRITE_TRACK stage=capability_probe_start scope=isolated_diagnostics");
    int major = 0, minor = 0;
    const bool version_known = kernel_version(&major, &minor);
    if (version_known && (major < 6 || (major == 6 && minor < 7))) {
        SDL_Log("ZA_WRITE_TRACK pagemap_scan_upstream_kernel_support=NO base_kernel_before_6_7 vendor_backport=unverified");
    } else {
        SDL_Log("ZA_WRITE_TRACK pagemap_scan_upstream_kernel_support=possible vendor_kernel_runtime_probe_required");
    }

    const uint64_t features = probe_userfaultfd_features();
    const bool wp_ready =
        (features & UFFD_FEATURE_PAGEFAULT_FLAG_WP) &&
        (features & UFFD_FEATURE_WP_ASYNC) &&
        (features & UFFD_FEATURE_WP_UNPOPULATED);

    const long page_size_long = sysconf(_SC_PAGESIZE);
    if (page_size_long <= 0) {
        errno = EINVAL;
        log_errno("tracking_page_size");
        return;
    }
    const size_t page_size = (size_t)page_size_long;

    probe_soft_dirty(page_size);
    const bool scan_callable = probe_pagemap_scan_generic(page_size);
    if (!wp_ready) {
        SDL_Log("ZA_WRITE_TRACK pagemap_scan_written=SKIPPED_MISSING_UFFD_WP_FEATURES");
    } else if (!scan_callable) {
        SDL_Log("ZA_WRITE_TRACK pagemap_scan_written=SKIPPED_GENERIC_SCAN_UNAVAILABLE");
    } else {
        SDL_Log("ZA_WRITE_TRACK pagemap_scan_written=NOT_IMPLEMENTED_IN_CAPABILITY_PROBE");
    }
    SDL_Log("ZA_WRITE_TRACK result=CAPABILITY_REPORT_COMPLETE");
}
