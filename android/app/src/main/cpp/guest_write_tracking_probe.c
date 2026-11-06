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

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif
#ifndef UFFD_USER_MODE_ONLY
#define UFFD_USER_MODE_ONLY 1
#endif
#ifndef UFFD_FEATURE_WP_UNPOPULATED
#define UFFD_FEATURE_WP_UNPOPULATED (1ULL << 13)
#endif
#ifndef UFFD_FEATURE_WP_ASYNC
#define UFFD_FEATURE_WP_ASYNC (1ULL << 15)
#endif

/* Linux fs.h PAGEMAP_SCAN UAPI, matching upstream runtime/platform/linux.c. */
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
#define ZA_SCAN_WP_MATCHING (1ULL << 0)
#define ZA_SCAN_CHECK_WPASYNC (1ULL << 1)

static void log_errno(const char *stage)
{
    SDL_Log("ZA_WRITE_TRACK stage=%s result=FAIL errno=%d message=%s",
            stage, errno, strerror(errno));
}

void za_run_guest_write_tracking_probe(void)
{
#if !defined(SYS_userfaultfd)
    SDL_Log("ZA_WRITE_TRACK result=UNAVAILABLE reason=SYS_userfaultfd_not_defined");
#else
    const uintptr_t guest_base = (uintptr_t)UINT64_C(0x7000000000);
    const size_t guest_size = (size_t)UINT64_C(0x800000000);
    const size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
    void *guest = MAP_FAILED;
    int uffd = -1;
    int pagemap = -1;
    bool registered = false;
    bool pass = false;
    const uint64_t api_features = UFFD_FEATURE_WP_ASYNC | UFFD_FEATURE_WP_UNPOPULATED;

    SDL_Log("ZA_WRITE_TRACK stage=start base=0x%llx size=%llu page_size=%llu",
            (unsigned long long)guest_base,
            (unsigned long long)guest_size,
            (unsigned long long)page_size);

    if (page_size == 0) {
        errno = EINVAL;
        log_errno("page_size");
        goto cleanup;
    }

    guest = mmap((void *)guest_base, guest_size, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE,
                 -1, 0);
    if (guest == MAP_FAILED) {
        log_errno("reserve_guest_window");
        goto cleanup;
    }
    if (guest != (void *)guest_base) {
        SDL_Log("ZA_WRITE_TRACK stage=reserve_guest_window result=FAIL reason=wrong_address actual=%p", guest);
        goto cleanup;
    }
    SDL_Log("ZA_WRITE_TRACK stage=reserve_guest_window result=PASS");

    uffd = (int)syscall(SYS_userfaultfd,
                        O_CLOEXEC | O_NONBLOCK | UFFD_USER_MODE_ONLY);
    if (uffd < 0) {
        log_errno("userfaultfd_syscall");
        goto cleanup;
    }
    SDL_Log("ZA_WRITE_TRACK stage=userfaultfd_open result=PASS fd=%d", uffd);

    struct uffdio_api api = {
        .api = UFFD_API,
        .features = api_features
    };
    if (ioctl(uffd, UFFDIO_API, &api) < 0) {
        log_errno("uffdio_api_async_wp");
        goto cleanup;
    }
    SDL_Log("ZA_WRITE_TRACK stage=uffdio_api result=PASS requested=0x%llx negotiated=0x%llx",
            (unsigned long long)api_features,
            (unsigned long long)api.features);
    if ((api.features & api_features) != api_features) {
        SDL_Log("ZA_WRITE_TRACK stage=uffdio_api result=FAIL reason=required_features_missing");
        goto cleanup;
    }

    struct uffdio_register registration = {
        .range = { .start = (uint64_t)guest_base, .len = (uint64_t)guest_size },
        .mode = UFFDIO_REGISTER_MODE_WP
    };
    if (ioctl(uffd, UFFDIO_REGISTER, &registration) < 0) {
        log_errno("uffdio_register_32gib");
        goto cleanup;
    }
    registered = true;
    SDL_Log("ZA_WRITE_TRACK stage=uffdio_register_32gib result=PASS ioctls=0x%llx",
            (unsigned long long)registration.ioctls);

    pagemap = open("/proc/self/pagemap", O_RDONLY | O_CLOEXEC);
    if (pagemap < 0) {
        log_errno("open_proc_self_pagemap");
        goto cleanup;
    }
    SDL_Log("ZA_WRITE_TRACK stage=open_pagemap result=PASS");

    struct uffdio_writeprotect writeprotect = {
        .range = { .start = (uint64_t)guest_base, .len = (uint64_t)page_size },
        .mode = UFFDIO_WRITEPROTECT_MODE_WP
    };
    if (ioctl(uffd, UFFDIO_WRITEPROTECT, &writeprotect) < 0) {
        log_errno("uffdio_writeprotect");
        goto cleanup;
    }
    SDL_Log("ZA_WRITE_TRACK stage=writeprotect_page result=PASS");

    volatile uint32_t *probe_word = (volatile uint32_t *)guest;
    *probe_word = UINT32_C(0x5A413057);

    ZaWtRegion regions[4];
    ZaWtScan scan = {
        .size = sizeof(scan),
        .flags = ZA_SCAN_WP_MATCHING | ZA_SCAN_CHECK_WPASYNC,
        .start = (uint64_t)guest_base,
        .end = (uint64_t)guest_base + (uint64_t)page_size,
        .vec = (uint64_t)(uintptr_t)regions,
        .vec_len = 4,
        .category_mask = ZA_PAGE_IS_WRITTEN,
        .return_mask = ZA_PAGE_IS_WRITTEN
    };
    int found = ioctl(pagemap, ZA_PAGEMAP_SCAN, &scan);
    if (found < 0) {
        log_errno("pagemap_scan_after_write");
        goto cleanup;
    }
    bool page_reported = false;
    for (int i = 0; i < found; ++i) {
        if (regions[i].start <= (uint64_t)guest_base &&
            regions[i].end >= (uint64_t)guest_base + (uint64_t)page_size &&
            (regions[i].categories & ZA_PAGE_IS_WRITTEN) != 0) {
            page_reported = true;
        }
    }
    if (!page_reported) {
        SDL_Log("ZA_WRITE_TRACK stage=pagemap_scan_after_write result=FAIL regions=%d walk_end=0x%llx",
                found, (unsigned long long)scan.walk_end);
        goto cleanup;
    }
    SDL_Log("ZA_WRITE_TRACK stage=pagemap_scan_detected_write result=PASS regions=%d", found);

    scan.flags = ZA_SCAN_WP_MATCHING | ZA_SCAN_CHECK_WPASYNC;
    scan.start = (uint64_t)guest_base;
    scan.end = (uint64_t)guest_base + (uint64_t)page_size;
    scan.walk_end = 0;
    scan.vec_len = 4;
    int reset_found = ioctl(pagemap, ZA_PAGEMAP_SCAN, &scan);
    if (reset_found < 0) {
        log_errno("pagemap_scan_reset");
        goto cleanup;
    }
    if (reset_found != 0) {
        SDL_Log("ZA_WRITE_TRACK stage=pagemap_scan_reset result=FAIL remaining_regions=%d", reset_found);
        goto cleanup;
    }
    SDL_Log("ZA_WRITE_TRACK stage=pagemap_scan_reset result=PASS");
    pass = true;

cleanup:
    if (pagemap >= 0) close(pagemap);
    if (uffd >= 0) {
        if (registered) {
            struct uffdio_range range = {
                .start = (uint64_t)guest_base,
                .len = (uint64_t)guest_size
            };
            (void)ioctl(uffd, UFFDIO_UNREGISTER, &range);
        }
        close(uffd);
    }
    if (guest != MAP_FAILED) (void)munmap(guest, guest_size);

    if (pass) {
        SDL_Log("ZA_WRITE_TRACK result=PASS stages=reserve_32gib,userfaultfd,async_wp,register_32gib,pagemap_scan,detect_write,reset_write_tracking,cleanup");
    } else {
        SDL_Log("ZA_WRITE_TRACK result=UNSUPPORTED_OR_FAILED; guest mapping and descriptors cleaned up");
    }
#endif
}
