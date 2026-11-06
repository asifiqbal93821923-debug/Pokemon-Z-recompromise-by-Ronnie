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
#include <unistd.h>
#include <linux/userfaultfd.h>

#include "guest_write_tracking_probe.h"

#ifndef UFFD_USER_MODE_ONLY
#define UFFD_USER_MODE_ONLY 1
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

/* Linux fs.h PAGEMAP_SCAN UAPI. Keep the layout aligned with pm_scan_arg. */
typedef struct {
    uint64_t start;
    uint64_t end;
    uint64_t categories;
} ZaWtRegion;

typedef struct {
    uint64_t size;
    uint64_t flags;
    uint64_t start;
    uint64_t end;
    uint64_t walk_end;
    uint64_t vec;
    uint64_t vec_len;
    uint64_t max_pages;
    uint64_t category_inverted;
    uint64_t category_mask;
    uint64_t category_anyof_mask;
    uint64_t return_mask;
} ZaWtScan;

#define ZA_PAGEMAP_SCAN _IOWR('f', 16, ZaWtScan)
#define ZA_PAGE_IS_WRITTEN (1ULL << 1)
#define ZA_SOFT_DIRTY_BIT (1ULL << 55)

static void log_errno(const char *stage)
{
    SDL_Log("ZA_WRITE_TRACK stage=%s result=FAIL errno=%d message=%s",
            stage, errno, strerror(errno));
}

#if defined(SYS_userfaultfd)
static int open_userfaultfd(void)
{
    return (int)syscall(SYS_userfaultfd,
                        O_CLOEXEC | O_NONBLOCK | UFFD_USER_MODE_ONLY);
}

static bool probe_uffd_feature(uint64_t feature, const char *name)
{
    int fd = open_userfaultfd();
    if (fd < 0) {
        log_errno("feature_probe_open");
        return false;
    }

    struct uffdio_api api = {
        .api = UFFD_API,
        .features = feature
    };
    if (ioctl(fd, UFFDIO_API, &api) < 0) {
        char stage[96];
        (void)snprintf(stage, sizeof(stage), "feature_%s_api", name);
        log_errno(stage);
        close(fd);
        return false;
    }

    bool enabled = (api.features & feature) == feature;
    SDL_Log("ZA_WRITE_TRACK feature=%s result=%s negotiated=0x%llx",
            name, enabled ? "PASS" : "NOT_ENABLED",
            (unsigned long long)api.features);
    close(fd);
    return enabled;
}
#endif

static void probe_userfaultfd_features(void)
{
#if !defined(SYS_userfaultfd)
    SDL_Log("ZA_WRITE_TRACK userfaultfd=UNAVAILABLE reason=SYS_userfaultfd_not_defined");
#else
    int fd = open_userfaultfd();
    if (fd < 0) {
        log_errno("userfaultfd_open");
        SDL_Log("ZA_WRITE_TRACK userfaultfd=UNAVAILABLE");
        return;
    }
    SDL_Log("ZA_WRITE_TRACK userfaultfd_open=PASS fd=%d", fd);

    /*
     * UFFDIO_API is a one-time initialization ioctl per descriptor.
     * Query with features=0, close this descriptor, then test each
     * advertised feature on a fresh descriptor.
     */
    struct uffdio_api query = {
        .api = UFFD_API,
        .features = 0
    };
    if (ioctl(fd, UFFDIO_API, &query) < 0) {
        log_errno("uffdio_api_query_features_zero");
        close(fd);
        return;
    }

    const uint64_t available = query.features;
    SDL_Log("ZA_WRITE_TRACK uffdio_api_query=PASS available_features=0x%llx generic_ioctls=0x%llx",
            (unsigned long long)available,
            (unsigned long long)query.ioctls);
    close(fd);

    const bool wp_async_advertised =
        (available & UFFD_FEATURE_WP_ASYNC) != 0;
    const bool wp_unpopulated_advertised =
        (available & UFFD_FEATURE_WP_UNPOPULATED) != 0;

    SDL_Log("ZA_WRITE_TRACK feature_advertised=WP_ASYNC value=%s",
            wp_async_advertised ? "yes" : "no");
    SDL_Log("ZA_WRITE_TRACK feature_advertised=WP_UNPOPULATED value=%s",
            wp_unpopulated_advertised ? "yes" : "no");

    if (wp_async_advertised)
        (void)probe_uffd_feature(UFFD_FEATURE_WP_ASYNC, "WP_ASYNC");
    else
        SDL_Log("ZA_WRITE_TRACK feature=WP_ASYNC result=UNSUPPORTED_NOT_ADVERTISED");

    if (wp_unpopulated_advertised)
        (void)probe_uffd_feature(UFFD_FEATURE_WP_UNPOPULATED, "WP_UNPOPULATED");
    else
        SDL_Log("ZA_WRITE_TRACK feature=WP_UNPOPULATED result=UNSUPPORTED_NOT_ADVERTISED");
#endif
}

static void probe_pagemap_scan_and_soft_dirty(void)
{
    const long page_size_long = sysconf(_SC_PAGESIZE);
    if (page_size_long <= 0) {
        errno = EINVAL;
        log_errno("tracking_page_size");
        return;
    }

    const size_t page_size = (size_t)page_size_long;
    void *page = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED) {
        log_errno("tracking_test_page_mmap");
        return;
    }

    volatile uint32_t *word = (volatile uint32_t *)page;
    *word = UINT32_C(0x5A413031);

    int clear_refs = open("/proc/self/clear_refs", O_WRONLY | O_CLOEXEC);
    if (clear_refs < 0) {
        log_errno("soft_dirty_open_clear_refs");
    } else {
        const char clear_command[] = "4";
        ssize_t written = write(clear_refs, clear_command, 1);
        if (written != 1) {
            if (written >= 0) errno = EIO;
            log_errno("soft_dirty_clear_refs_write");
        } else {
            *word = UINT32_C(0x5A413032);

            int pagemap = open("/proc/self/pagemap", O_RDONLY | O_CLOEXEC);
            if (pagemap < 0) {
                log_errno("soft_dirty_open_pagemap");
            } else {
                const uint64_t page_index =
                    (uint64_t)(uintptr_t)page / (uint64_t)page_size;
                const off_t offset = (off_t)(page_index * sizeof(uint64_t));
                uint64_t entry = 0;
                ssize_t count = pread(pagemap, &entry, sizeof(entry), offset);
                if (count != (ssize_t)sizeof(entry)) {
                    if (count >= 0) errno = EIO;
                    log_errno("soft_dirty_read_pagemap_entry");
                } else {
                    const bool dirty = (entry & ZA_SOFT_DIRTY_BIT) != 0;
                    SDL_Log("ZA_WRITE_TRACK soft_dirty_bit55=%s pagemap_entry=0x%llx",
                            dirty ? "SET" : "NOT_SET",
                            (unsigned long long)entry);
                    SDL_Log("ZA_WRITE_TRACK soft_dirty_result=%s",
                            dirty ? "PASS" : "FAIL");
                }
                close(pagemap);
            }
        }
        close(clear_refs);
    }

    /*
     * PAGEMAP_SCAN is independent of userfaultfd. The test page is
     * written after clear_refs above; unsupported kernels commonly
     * return ENOTTY for this ioctl.
     */
    int pagemap_scan_fd = open("/proc/self/pagemap", O_RDONLY | O_CLOEXEC);
    if (pagemap_scan_fd < 0) {
        log_errno("pagemap_scan_open_pagemap");
    } else {
        ZaWtRegion regions[4];
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
            .category_mask = ZA_PAGE_IS_WRITTEN,
            .category_anyof_mask = 0,
            .return_mask = ZA_PAGE_IS_WRITTEN
        };
        long found = ioctl(pagemap_scan_fd, ZA_PAGEMAP_SCAN, &scan);
        if (found < 0) {
            log_errno("pagemap_scan_ioctl");
            if (errno == ENOTTY || errno == EINVAL)
                SDL_Log("ZA_WRITE_TRACK pagemap_scan_result=UNSUPPORTED_OR_UAPI_MISMATCH");
        } else {
            bool written_page_found = false;
            for (long i = 0; i < found && i < 4; ++i) {
                if (regions[i].start <= (uint64_t)(uintptr_t)page &&
                    regions[i].end >= (uint64_t)(uintptr_t)page + (uint64_t)page_size &&
                    (regions[i].categories & ZA_PAGE_IS_WRITTEN) != 0) {
                    written_page_found = true;
                    break;
                }
            }
            SDL_Log("ZA_WRITE_TRACK pagemap_scan_ioctl=PASS regions=%ld walk_end=0x%llx written_page=%s",
                    found, (unsigned long long)scan.walk_end,
                    written_page_found ? "yes" : "no");
            SDL_Log("ZA_WRITE_TRACK pagemap_scan_result=%s",
                    written_page_found ? "PASS" : "NO_WRITTEN_PAGE_REPORTED");
        }
        close(pagemap_scan_fd);
    }

    (void)munmap(page, page_size);
    SDL_Log("ZA_WRITE_TRACK pagemap_soft_dirty_probe_cleanup=PASS");
}

void za_run_guest_write_tracking_probe(void)
{
    SDL_Log("ZA_WRITE_TRACK stage=capability_probe_start page_scope=one_page");
    probe_userfaultfd_features();
    probe_pagemap_scan_and_soft_dirty();
    SDL_Log("ZA_WRITE_TRACK result=CAPABILITY_REPORT_COMPLETE");
}
