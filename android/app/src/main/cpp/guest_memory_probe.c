#include "guest_memory_probe.h"

#include <SDL3/SDL.h>
#include <stdbool.h>

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef MAP_FIXED_NOREPLACE
/* Linux UAPI value; older kernels may ignore it, so the returned address is checked. */
#define MAP_FIXED_NOREPLACE 0x100000
#endif

#define ZA_GUEST_BASE UINT64_C(0x7000000000)
#define ZA_GUEST_SIZE UINT64_C(0x800000000)
#define ZA_TEST_PATTERN UINT32_C(0x5A41304D)

void za_run_guest_memory_probe(void)
{
    const size_t guest_size = (size_t)ZA_GUEST_SIZE;
    const uintptr_t guest_base = (uintptr_t)ZA_GUEST_BASE;
    const long page_size_long = sysconf(_SC_PAGESIZE);

    SDL_Log("ZA_GUEST_MEM stage=start base=0x%" PRIxPTR
            " size=%zu page_size=%ld",
            guest_base, guest_size, page_size_long);

    if (sizeof(void *) != 8 || page_size_long <= 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ZA_GUEST_MEM result=unsupported pointer_size=%zu page_size=%ld",
                     sizeof(void *), page_size_long);
        return;
    }

    const size_t page_size = (size_t)page_size_long;
    if ((guest_base % page_size) != 0 || (guest_size % page_size) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ZA_GUEST_MEM result=invalid_alignment");
        return;
    }

    void *requested = (void *)guest_base;
    void *reserved = mmap(requested, guest_size, PROT_NONE,
                          MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                          -1, 0);
    if (reserved == MAP_FAILED) {
        const int saved_errno = errno;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ZA_GUEST_MEM result=reserve_failed errno=%d message=%s",
                     saved_errno, strerror(saved_errno));
        return;
    }

    if (reserved != requested) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ZA_GUEST_MEM result=unsafe_address_returned requested=%p actual=%p",
                     requested, reserved);
        (void)munmap(reserved, guest_size);
        return;
    }
    SDL_Log("ZA_GUEST_MEM stage=exact_32gib_reservation_pass address=%p", reserved);

    bool writable = mprotect(reserved, page_size, PROT_READ | PROT_WRITE) == 0;
    if (!writable) {
        const int saved_errno = errno;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ZA_GUEST_MEM result=commit_rw_failed errno=%d message=%s",
                     saved_errno, strerror(saved_errno));
        (void)munmap(reserved, guest_size);
        return;
    }

    volatile uint32_t *probe_word = (volatile uint32_t *)reserved;
    *probe_word = ZA_TEST_PATTERN;

    if (mprotect(reserved, page_size, PROT_READ) != 0) {
        const int saved_errno = errno;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ZA_GUEST_MEM result=protect_read_failed errno=%d message=%s",
                     saved_errno, strerror(saved_errno));
        (void)munmap(reserved, guest_size);
        return;
    }

    const uint32_t observed = *probe_word;
    if (observed != ZA_TEST_PATTERN) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ZA_GUEST_MEM result=readback_mismatch expected=0x%08" PRIx32
                     " actual=0x%08" PRIx32,
                     ZA_TEST_PATTERN, observed);
        (void)munmap(reserved, guest_size);
        return;
    }
    SDL_Log("ZA_GUEST_MEM stage=commit_write_readback_protect_pass");

    if (mprotect(reserved, page_size, PROT_NONE) != 0) {
        const int saved_errno = errno;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ZA_GUEST_MEM result=protect_none_failed errno=%d message=%s",
                     saved_errno, strerror(saved_errno));
        (void)munmap(reserved, guest_size);
        return;
    }
    SDL_Log("ZA_GUEST_MEM stage=protect_none_pass");

    if (munmap(reserved, guest_size) != 0) {
        const int saved_errno = errno;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ZA_GUEST_MEM result=release_failed errno=%d message=%s",
                     saved_errno, strerror(saved_errno));
        return;
    }

    SDL_Log("ZA_GUEST_MEM result=PASS stages=reserve_32gib,commit_page,"
            "write_readback,protect_read,protect_none,release");
}
