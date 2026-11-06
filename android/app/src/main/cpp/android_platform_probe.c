#include "android_platform_probe.h"
#include "platform.h"
#include <errno.h>
#include <stdint.h>
#include <sys/mman.h>
#include <time.h>
#include <SDL3/SDL.h>

#define ZA_GUEST_LO 0x7000000000ull
#define ZA_GUEST_HI 0x7800000000ull
#define ZA_PROBE_PATTERN 0x5A41304Du

int za_run_android_platform_api_probe(void)
{
    const uint64_t page = 4096;
    SDL_Log("ZA_PLATFORM_API stage=start");

    if (!plat_reserve_guest(ZA_GUEST_LO, ZA_GUEST_HI)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ZA_PLATFORM_API stage=reserve result=FAIL errno=%d", errno);
        return 1;
    }
    SDL_Log("ZA_PLATFORM_API stage=reserve_32gib result=PASS");

    plat_commit(ZA_GUEST_LO, page);
    volatile uint32_t *word = (volatile uint32_t *)(uintptr_t)ZA_GUEST_LO;
    *word = ZA_PROBE_PATTERN;
    if (*word != ZA_PROBE_PATTERN) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "ZA_PLATFORM_API stage=commit_write result=FAIL");
        (void)munmap((void *)(uintptr_t)ZA_GUEST_LO, (size_t)(ZA_GUEST_HI - ZA_GUEST_LO));
        return 2;
    }
    if (plat_protect(ZA_GUEST_LO, page, PLAT_PROT_READ) != 0 || *word != ZA_PROBE_PATTERN) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ZA_PLATFORM_API stage=protect_read result=FAIL errno=%d", errno);
        (void)munmap((void *)(uintptr_t)ZA_GUEST_LO, (size_t)(ZA_GUEST_HI - ZA_GUEST_LO));
        return 3;
    }
    SDL_Log("ZA_PLATFORM_API stage=commit_write_read_protect result=PASS");

    if (plat_protect(ZA_GUEST_LO, page, PLAT_PROT_NONE) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ZA_PLATFORM_API stage=protect_none result=FAIL errno=%d", errno);
        (void)munmap((void *)(uintptr_t)ZA_GUEST_LO, (size_t)(ZA_GUEST_HI - ZA_GUEST_LO));
        return 4;
    }
    if (!plat_reset_guest(ZA_GUEST_LO, ZA_GUEST_LO + page)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ZA_PLATFORM_API stage=reset_page result=FAIL errno=%d", errno);
        (void)munmap((void *)(uintptr_t)ZA_GUEST_LO, (size_t)(ZA_GUEST_HI - ZA_GUEST_LO));
        return 5;
    }
    if (*word != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "ZA_PLATFORM_API stage=reset_zero result=FAIL");
        (void)munmap((void *)(uintptr_t)ZA_GUEST_LO, (size_t)(ZA_GUEST_HI - ZA_GUEST_LO));
        return 6;
    }
    SDL_Log("ZA_PLATFORM_API stage=reset_zero result=PASS");

    errno = 0;
    if (plat_protect(ZA_GUEST_HI, page, PLAT_PROT_NONE) == 0 || errno != EINVAL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ZA_PLATFORM_API stage=reject_outside_range result=FAIL errno=%d", errno);
        (void)munmap((void *)(uintptr_t)ZA_GUEST_LO, (size_t)(ZA_GUEST_HI - ZA_GUEST_LO));
        return 7;
    }
    SDL_Log("ZA_PLATFORM_API stage=reject_outside_range result=PASS");

    const uint64_t before = plat_now_ns();
    const uint64_t cpu = plat_thread_cpu_ns();
    const int cpus = plat_cpu_count();
    if (!before || !cpu || cpus <= 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ZA_PLATFORM_API stage=time_cpu result=FAIL monotonic=%llu thread_cpu=%llu cpus=%d",
                     (unsigned long long)before, (unsigned long long)cpu, cpus);
        (void)munmap((void *)(uintptr_t)ZA_GUEST_LO, (size_t)(ZA_GUEST_HI - ZA_GUEST_LO));
        return 8;
    }
    plat_sleep_until_ns(before + 1000000ull);
    if (plat_now_ns() < before + 1000000ull) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "ZA_PLATFORM_API stage=absolute_sleep result=FAIL");
        (void)munmap((void *)(uintptr_t)ZA_GUEST_LO, (size_t)(ZA_GUEST_HI - ZA_GUEST_LO));
        return 9;
    }
    SDL_Log("ZA_PLATFORM_API stage=time_sleep_cpu_count result=PASS cpus=%d", cpus);

    if (munmap((void *)(uintptr_t)ZA_GUEST_LO, (size_t)(ZA_GUEST_HI - ZA_GUEST_LO)) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ZA_PLATFORM_API stage=release result=FAIL errno=%d", errno);
        return 10;
    }
    SDL_Log("ZA_PLATFORM_API result=PASS stages=reserve,commit,write_readback,protect_read,protect_none,reset_zero,reject_outside_range,monotonic,thread_cpu,absolute_sleep,cpu_count,release");
    return 0;
}
