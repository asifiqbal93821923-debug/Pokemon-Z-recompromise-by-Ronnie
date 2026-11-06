#include "android_platform_probe.h"
#include "platform.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "paths.h"
#include "kernel.h"
#include "android_fiber_arm64.h"
#include <sys/stat.h>
#include <stdint.h>
#include <sys/mman.h>
#include <time.h>
#include <SDL3/SDL.h>

#define ZA_GUEST_LO 0x7000000000ull
#define ZA_GUEST_HI 0x7800000000ull
#define ZA_PROBE_PATTERN 0x5A41304Du

static bool probe_filesystem_api(void)
{
    char *base = SDL_GetPrefPath("Ronnie", "ZAPlatformProbe");
    if (!base) return false;
    char dir[1024], first[1100], renamed[1100], occupied[1100];
    char target[1100], linkpath[1100];
    int nd = snprintf(dir, sizeof dir, "%sfs-api-probe", base);
    bool ok = nd > 0 && (size_t)nd < sizeof dir;
    int nf = snprintf(first, sizeof first, "%s/item.tmp", dir);
    int nr = snprintf(renamed, sizeof renamed, "%s/item.renamed", dir);
    int no = snprintf(occupied, sizeof occupied, "%s/occupied.tmp", dir);
    int nt = snprintf(target, sizeof target, "%sfs-probe-outside.tmp", base);
    int nl = snprintf(linkpath, sizeof linkpath, "%s/external-link", dir);
    ok = ok && nf > 0 && (size_t)nf < sizeof first &&
         nr > 0 && (size_t)nr < sizeof renamed &&
         no > 0 && (size_t)no < sizeof occupied &&
         nt > 0 && (size_t)nt < sizeof target &&
         nl > 0 && (size_t)nl < sizeof linkpath;
    SDL_free(base);
    if (!ok || mkdir(dir, 0700) != 0) return false;
    int fd = plat_open_file(first, O_CREAT | O_EXCL | O_WRONLY);
    const char payload[] = "ZA_ANDROID_PLATFORM_FS_V1";
    ok = fd >= 0 && write(fd, payload, sizeof payload) == (ssize_t)sizeof payload;
    if (fd >= 0 && close(fd) != 0) ok = false;
    if (ok) ok = !plat_is_link(first);
    if (ok) ok = plat_rename(first, renamed) == 0;
    int occupied_fd = plat_open_file(occupied, O_CREAT | O_EXCL | O_WRONLY);
    if (occupied_fd < 0) ok = false;
    if (occupied_fd >= 0) close(occupied_fd);
    if (ok) {
        errno = 0;
        ok = plat_rename(renamed, occupied) != 0 && errno == EEXIST;
    }
    int target_fd = plat_open_file(target, O_CREAT | O_EXCL | O_WRONLY);
    if (target_fd < 0) ok = false;
    if (target_fd >= 0 && close(target_fd) != 0) ok = false;
    if (ok && symlink(target, linkpath) != 0) ok = false;
    if (ok && !plat_is_link(linkpath)) ok = false;
    bool removed = plat_remove_tree(dir);
    bool target_preserved = access(target, F_OK) == 0;
    if (unlink(target) != 0) target_preserved = false;
    return ok && removed && target_preserved;
}

int za_run_android_platform_api_probe(void)
{
    const uint64_t page = 4096;
    SDL_Log("ZA_PLATFORM_API stage=start");

    plat_init();
    extern int za_android_platform_link_probe(void);
    const int linked_symbols = za_android_platform_link_probe();
    const char *xdg_data_home = getenv("XDG_DATA_HOME");
    const bool private_data_home_ok = xdg_data_home && xdg_data_home[0] == '/' && access(xdg_data_home, W_OK) == 0;
    if (linked_symbols != 39 || !private_data_home_ok) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ZA_ANDROID_RUNTIME_ENV result=FAIL linked_symbols=%d expected=39 xdg_data_home=%s",
                     linked_symbols, xdg_data_home ? xdg_data_home : "(null)");
        return 15;
    }
    SDL_Log("ZA_ANDROID_RUNTIME_ENV result=PASS linked_platform_symbols=%d app_private_xdg_data_home=yes", linked_symbols);
    if (!za_android_fiber_context_probe()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "ZA_ANDROID_FIBER result=FAIL mode=AARCH64_CONTEXT_SWITCH");
        return 17;
    }
    SDL_Log("ZA_ANDROID_FIBER result=PASS mode=AARCH64_CONTEXT_SWITCH");

    /* Exercise the pinned upstream path manager, not just the SDL private-dir API. */
    rt_paths_init();
    const char *runtime_root = rt_data_root();
    char expected_runtime_root[1200];
    const size_t xdg_len = strlen(xdg_data_home);
    int root_len = snprintf(expected_runtime_root, sizeof(expected_runtime_root),
                            "%s%szarecomp", xdg_data_home,
                            (xdg_len > 0 && xdg_data_home[xdg_len - 1] == '/') ? "" : "/");
    struct stat runtime_root_stat;
    if (root_len <= 0 || (size_t)root_len >= sizeof(expected_runtime_root) ||
        !runtime_root || strcmp(runtime_root, expected_runtime_root) != 0 ||
        stat(runtime_root, &runtime_root_stat) != 0 || !S_ISDIR(runtime_root_stat.st_mode) ||
        access(runtime_root, R_OK | W_OK) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ZA_RUNTIME_PATHS result=FAIL root=%s expected=%s errno=%d",
                     runtime_root ? runtime_root : "(null)", expected_runtime_root, errno);
        return 16;
    }
    SDL_Log("ZA_RUNTIME_PATHS result=PASS app_private_root=%s writable=yes",
            runtime_root);

    PlatSysInfo system_info = {0};
    char os_name[160] = {0};
    plat_system_info(&system_info);
    plat_os_name(os_name, sizeof os_name);
    if (!system_info.os[0] || !system_info.cpu[0] || !os_name[0] ||
        system_info.cores <= 0 || system_info.threads <= 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ZA_PLATFORM_API stage=system_info result=FAIL os=%s cpu=%s cores=%d threads=%d",
                     system_info.os, system_info.cpu, system_info.cores, system_info.threads);
        return 11;
    }
    SDL_Log("ZA_PLATFORM_API stage=system_info result=PASS os=%s cpu=%s cores=%d threads=%d ram=%llu",
            system_info.os, system_info.cpu, system_info.cores, system_info.threads,
            (unsigned long long)system_info.ram);
    if (plat_exe_path(os_name, sizeof os_name)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ZA_PLATFORM_API stage=exe_path result=FAIL unexpected desktop executable path");
        return 12;
    }
    SDL_Log("ZA_PLATFORM_API stage=exe_path result=UNSUPPORTED_EXPECTED Android APK assets require Java/AssetManager bridge");

    if (!plat_reserve_guest(ZA_GUEST_LO, ZA_GUEST_HI)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ZA_PLATFORM_API stage=reserve result=FAIL errno=%d", errno);
        return 1;
    }
    SDL_Log("ZA_PLATFORM_API stage=reserve_32gib result=PASS");

    /* Exercise real upstream kernel initialization with no game modules loaded.
       This is a core-runtime smoke check, not a game boot. */
    kernel_init();
    kernel_reset();
    SDL_Log("ZA_RUNTIME_KERNEL result=PASS init=PASS reset=PASS game_modules=0");

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

    /* Android 6.1 on the tested device has no usable kernel dirty-page API.
       Validate the upstream contract: -1 means tracking unavailable. The pinned
       Vulkan renderer AUTO path must then disable GPU overlap and use its normal
       same-frame fence wait; do not force overlap ON without validated tracking. */
    uint64_t dirty_pages[4] = {0};
    uint64_t dirty_page_size = 0;
    const int dirty_count = plat_written_pages(ZA_GUEST_LO, page,
                                                dirty_pages, 4, &dirty_page_size);
    const int dirty_peek = plat_written_pages_peek(ZA_GUEST_LO, page);
    if (dirty_count != -1 || dirty_peek != -1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ZA_WRITE_TRACK fallback result=FAIL written_pages=%d peek=%d",
                     dirty_count, dirty_peek);
        (void)munmap((void *)(uintptr_t)ZA_GUEST_LO, (size_t)(ZA_GUEST_HI - ZA_GUEST_LO));
        return 14;
    }
    SDL_Log("ZA_WRITE_TRACK fallback result=PASS mode=NO_TRACKER_RENDERER_AUTO_OVERLAP_OFF_REQUIRED written_pages=-1 peek=-1");


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

    if (!probe_filesystem_api()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "ZA_PLATFORM_API stage=filesystem result=FAIL errno=%d", errno);
        (void)munmap((void *)(uintptr_t)ZA_GUEST_LO, (size_t)(ZA_GUEST_HI - ZA_GUEST_LO));
        return 13;
    }
    SDL_Log("ZA_PLATFORM_API stage=filesystem result=PASS open,rename_noreplace,is_link,remove_tree");

    if (munmap((void *)(uintptr_t)ZA_GUEST_LO, (size_t)(ZA_GUEST_HI - ZA_GUEST_LO)) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ZA_PLATFORM_API stage=release result=FAIL errno=%d", errno);
        return 10;
    }
    SDL_Log("ZA_PLATFORM_API result=PASS stages=init,system_info,exe_path_unsupported,reserve,commit,write_readback,protect_read,protect_none,reset_zero,reject_outside_range,monotonic,thread_cpu,absolute_sleep,cpu_count,filesystem,release");
    return 0;
}
