#define _GNU_SOURCE
#include <android/log.h>
#include <dlfcn.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/system_properties.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <time.h>
#include <ucontext.h>
#include <unwind.h>
#include <unistd.h>
#include <SDL3/SDL.h>
#include "platform.h"

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif
#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0
#endif
#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1u)
#endif

#define ZA_LOG_TAG "ZAAndroidPlatform"
#pragma weak rt_fault_watch
#pragma weak rt_fault_report

static pthread_mutex_t g_guest_lock = PTHREAD_MUTEX_INITIALIZER;
static uint64_t g_guest_lo, g_guest_hi;
static _Thread_local void *g_alt_signal_stack;

static uint64_t page_size(void) {
    long value = sysconf(_SC_PAGESIZE);
    return value > 0 ? (uint64_t)value : 4096ull;
}
static bool page_aligned(uint64_t value) {
    uint64_t page = page_size();
    return page && value % page == 0;
}
static bool valid_range(uint64_t lo, uint64_t hi) {
    return lo < hi && hi - lo <= (uint64_t)SIZE_MAX &&
           page_aligned(lo) && page_aligned(hi);
}
static bool inside_guest(uint64_t addr, uint64_t size) {
    return g_guest_lo && size && addr >= g_guest_lo && addr <= g_guest_hi &&
           size <= g_guest_hi - addr;
}

bool plat_reserve_guest(uint64_t lo, uint64_t hi) {
    if (!valid_range(lo, hi)) { errno = EINVAL; return false; }
    pthread_mutex_lock(&g_guest_lock);
    if (g_guest_lo || g_guest_hi) {
        pthread_mutex_unlock(&g_guest_lock);
        errno = EBUSY;
        return false;
    }
    void *wanted = (void *)(uintptr_t)lo;
    void *mapped = mmap(wanted, (size_t)(hi - lo), PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
    bool ok = mapped == wanted;
    if (mapped != MAP_FAILED && mapped != wanted)
        (void)munmap(mapped, (size_t)(hi - lo));
    if (ok) { g_guest_lo = lo; g_guest_hi = hi; }
    else if (mapped != MAP_FAILED) errno = EADDRNOTAVAIL;
    pthread_mutex_unlock(&g_guest_lock);
    return ok;
}
void plat_commit(uint64_t addr, uint64_t size) {
    pthread_mutex_lock(&g_guest_lock);
    bool valid = inside_guest(addr, size) && page_aligned(addr) && page_aligned(size);
    pthread_mutex_unlock(&g_guest_lock);
    if (!valid) errno = EINVAL;
    /* Anonymous MAP_NORESERVE pages are backed lazily on first access. */
}
bool plat_reset_guest(uint64_t lo, uint64_t hi) {
    if (!valid_range(lo, hi)) { errno = EINVAL; return false; }
    pthread_mutex_lock(&g_guest_lock);
    bool inside = g_guest_lo && lo >= g_guest_lo && hi <= g_guest_hi;
    if (!inside) {
        pthread_mutex_unlock(&g_guest_lock);
        errno = EINVAL;
        return false;
    }
    void *wanted = (void *)(uintptr_t)lo;
    void *mapped = mmap(wanted, (size_t)(hi - lo), PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED, -1, 0);
    bool ok = mapped == wanted;
    if (mapped != MAP_FAILED && mapped != wanted) {
        (void)munmap(mapped, (size_t)(hi - lo));
        errno = EADDRNOTAVAIL;
    }
    pthread_mutex_unlock(&g_guest_lock);
    return ok;
}

/* Android 16 / kernel 6.1 on the tested device lacks the async userfaultfd-WP
   prerequisites; PAGEMAP_SCAN is policy-blocked. Upstream -1 means unavailable.
   The renderer AUTO path must disable GPU overlap and retain normal fence waits. */
int plat_written_pages(uint64_t a, uint64_t s, uint64_t *p, int m, uint64_t *pg) {
    (void)a; (void)s; (void)p; (void)m; (void)pg;
    return -1;
}
int plat_written_pages_peek(uint64_t a, uint64_t s) { (void)a; (void)s; return -1; }
void plat_write_tracking_off(const char *why) {
    if (why && *why) __android_log_print(ANDROID_LOG_INFO, ZA_LOG_TAG,
                                        "write tracking unavailable: %s", why);
}
bool plat_page_state(uint64_t a, char *out, size_t n) {
    (void)a;
    if (out && n) out[0] = 0;
    return false;
}
bool plat_mem_usage(uint64_t *p, uint64_t *g) {
    if (p) *p = 0;
    if (g) *g = 0;
    return false;
}
void plat_sample_focus(void) {}
int plat_protect(uint64_t a, uint64_t s, int prot) {
    int p;
    switch (prot) {
        case PLAT_PROT_NONE: p = PROT_NONE; break;
        case PLAT_PROT_READ: p = PROT_READ; break;
        case PLAT_PROT_RW: p = PROT_READ | PROT_WRITE; break;
        default: errno = EINVAL; return -1;
    }
    if (!s || !page_aligned(a) || !page_aligned(s)) { errno = EINVAL; return -1; }
    pthread_mutex_lock(&g_guest_lock);
    bool inside = inside_guest(a, s);
    int result = inside ? mprotect((void *)(uintptr_t)a, (size_t)s, p) : -1;
    pthread_mutex_unlock(&g_guest_lock);
    if (!inside) errno = EINVAL;
    return result;
}

uint64_t plat_now_ns(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) return 0;
    return (uint64_t)t.tv_sec * 1000000000ull + (uint64_t)t.tv_nsec;
}
uint64_t plat_thread_cpu_ns(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t)) return 0;
    return (uint64_t)t.tv_sec * 1000000000ull + (uint64_t)t.tv_nsec;
}
void plat_sleep_until_ns(uint64_t deadline) {
    struct timespec t = {(time_t)(deadline / 1000000000ull),
                         (long)(deadline % 1000000000ull)};
    int result;
    do { result = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL); }
    while (result == EINTR);
}
int plat_cpu_count(void) {
    cpu_set_t set;
    if (sched_getaffinity(0, sizeof(set), &set) == 0) {
        int count = 0;
        for (int i = 0; i < CPU_SETSIZE; ++i) if (CPU_ISSET(i, &set)) ++count;
        return count;
    }
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    return count > 0 && count <= INT32_MAX ? (int)count : 0;
}
void plat_thread_guest(int priority) { (void)priority; }
void plat_thread_realtime(uint64_t period_ns) {
    (void)period_ns;
    struct sched_param param = {.sched_priority = 1};
    (void)pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
}
int plat_thread_realtime_check(uint64_t period_ns) { (void)period_ns; return -1; }

void plat_init(void) {
    /* Upstream paths.c chooses XDG_DATA_HOME/zarecomp on Linux. Keep that root
       inside this Android package's private SDL preference directory. */
    if (!getenv("LARECOMP_DATA_HOME")) {
        char *private_base = SDL_GetPrefPath("Ronnie", "PokemonZARecomp");
        if (private_base) {
            if (setenv("XDG_DATA_HOME", private_base, 1) != 0)
                __android_log_print(ANDROID_LOG_ERROR, ZA_LOG_TAG,
                                    "setenv(XDG_DATA_HOME) failed: errno=%d", errno);
            SDL_free(private_base);
        } else {
            __android_log_write(ANDROID_LOG_ERROR, ZA_LOG_TAG,
                                "SDL_GetPrefPath failed; app-private data root unavailable");
        }
    }
    (void)umask(0077);
}
void plat_system_info(PlatSysInfo *si) {
    if (!si) return;
    memset(si, 0, sizeof(*si));
    struct utsname u;
    memset(&u, 0, sizeof(u));
    bool have_uname = uname(&u) == 0;
    char release[PROP_VALUE_MAX] = {0};
    if (__system_property_get("ro.build.version.release", release) <= 0)
        snprintf(release, sizeof(release), "unknown");
    snprintf(si->os, sizeof(si->os), "Android %s (%s, kernel %.32s)",
             release, have_uname ? u.machine : "arm64",
             have_uname ? u.release : "unknown");
    snprintf(si->cpu, sizeof(si->cpu), "%s", have_uname ? u.machine : "ARM64");
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    if (count > 0 && count <= INT32_MAX) {
        si->cores = (int)count; /* Physical topology is not consistently exposed. */
        si->threads = (int)count;
    }
    struct sysinfo memory;
    if (sysinfo(&memory) == 0)
        si->ram = (uint64_t)memory.totalram * (uint64_t)memory.mem_unit;
    si->package = NULL;
}
bool plat_exe_path(char *buf, size_t size) {
    /* /proc/self/exe resolves to app_process, not an APK resource directory. */
    if (buf && size) buf[0] = 0;
    return false;
}
void plat_os_name(char *buf, size_t size) {
    if (!buf || !size) return;
    char release[PROP_VALUE_MAX] = {0};
    if (__system_property_get("ro.build.version.release", release) <= 0)
        snprintf(release, sizeof(release), "Android");
    struct utsname u;
    if (uname(&u) == 0) snprintf(buf, size, "Android %s (kernel %s)", release, u.release);
    else snprintf(buf, size, "Android %s", release);
}

/* Desktop process replacement and helper subprocesses are not implemented in
   this app target. Fail explicitly instead of pretending a process exists. */
void plat_exec(char *const argv[]) {
    (void)argv;
    errno = ENOTSUP;
    __android_log_write(ANDROID_LOG_ERROR, ZA_LOG_TAG, "plat_exec unsupported: use Android Activity lifecycle");
}
bool plat_relaunch(void) { errno = ENOTSUP; return false; }
void plat_terminate(void) { _exit(0); }
PlatProc plat_spawn(const char *const argv[], const char *log_path, bool append_log) {
    (void)argv; (void)log_path; (void)append_log;
    errno = ENOTSUP;
    __android_log_write(ANDROID_LOG_WARN, ZA_LOG_TAG,
                        "plat_spawn unsupported: helper-process integration pending");
    return (PlatProc)0;
}
int plat_proc_wait(PlatProc process, bool block) {
    (void)block;
    errno = process == 0 ? EINVAL : ENOTSUP;
    return 255;
}
void plat_proc_kill(PlatProc process) {
    errno = process == 0 ? EINVAL : ENOTSUP;
}

static bool remove_tree_entry(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return errno == ENOENT;
    if (!S_ISDIR(st.st_mode)) return unlink(path) == 0 || errno == ENOENT;
    DIR *dir = opendir(path);
    if (!dir) return false;
    bool ok = true;
    int saved_errno = 0;
    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(dir);
        if (!entry) { if (errno) { ok = false; saved_errno = errno; } break; }
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        char child[4096];
        int n = snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        if (n < 0 || (size_t)n >= sizeof(child) || !remove_tree_entry(child)) {
            ok = false; saved_errno = errno ? errno : ENAMETOOLONG; break;
        }
    }
    if (closedir(dir) != 0 && ok) { ok = false; saved_errno = errno; }
    if (!ok) { errno = saved_errno ? saved_errno : EIO; return false; }
    return rmdir(path) == 0 || errno == ENOENT;
}
bool plat_remove_tree(const char *path) {
    if (!path || !*path) { errno = EINVAL; return false; }
    return remove_tree_entry(path);
}
bool plat_is_link(const char *path) {
    struct stat st;
    return path && lstat(path, &st) == 0 &&
           !S_ISDIR(st.st_mode) && !S_ISREG(st.st_mode);
}
bool plat_onedrive_synced(const char *path) { (void)path; return false; }
int plat_open_file(const char *path, int flags) {
    if (!path) { errno = EINVAL; return -1; }
    return open(path, flags | O_CLOEXEC, 0644);
}
int plat_rename(const char *from, const char *to) {
    if (!from || !to) { errno = EINVAL; return -1; }
#if defined(SYS_renameat2)
    return (int)syscall(SYS_renameat2, AT_FDCWD, from, AT_FDCWD, to, RENAME_NOREPLACE);
#else
    struct stat st;
    if (lstat(to, &st) == 0) { errno = EEXIST; return -1; }
    if (errno != ENOENT) return -1;
    return rename(from, to);
#endif
}

const char *plat_symbol(uint64_t pc, uint64_t *offset) {
    Dl_info info;
    memset(&info, 0, sizeof(info));
    if (!dladdr((void *)(uintptr_t)pc, &info) || !info.dli_sname) return NULL;
    if (offset) *offset = pc - (uint64_t)(uintptr_t)info.dli_saddr;
    return info.dli_sname;
}
bool plat_module_name(uint64_t pc, char *out, size_t size) {
    if (!out || !size) return false;
    Dl_info info;
    memset(&info, 0, sizeof(info));
    if (!dladdr((void *)(uintptr_t)pc, &info) || !info.dli_fname) return false;
    int n = snprintf(out, size, "%s", info.dli_fname);
    return n >= 0 && (size_t)n < size;
}
typedef struct { int frames; } ZaBacktraceState;
static _Unwind_Reason_Code unwind_frame(struct _Unwind_Context *ctx, void *arg) {
    ZaBacktraceState *state = (ZaBacktraceState *)arg;
    uintptr_t pc = (uintptr_t)_Unwind_GetIP(ctx);
    if (pc) {
        __android_log_print(ANDROID_LOG_ERROR, ZA_LOG_TAG,
                            "backtrace[%d] pc=%p", state->frames, (void *)pc);
        ++state->frames;
    }
    return state->frames >= 64 ? _URC_END_OF_STACK : _URC_NO_REASON;
}
void plat_print_backtrace(void) {
    ZaBacktraceState state = {0};
    (void)_Unwind_Backtrace(unwind_frame, &state);
    if (!state.frames)
        __android_log_write(ANDROID_LOG_ERROR, ZA_LOG_TAG, "native backtrace unavailable");
}
void plat_fatal_notice(void) {
    const char *message = "ZA Recomp encountered a fatal error. See the application diagnostic log.";
    __android_log_write(ANDROID_LOG_FATAL, ZA_LOG_TAG, message);
    if (SDL_WasInit(SDL_INIT_VIDEO))
        (void)SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "ZA Recomp", message, NULL);
}
static void *notice_thread(void *arg) {
    char *message = (char *)arg;
    if (message && SDL_WasInit(SDL_INIT_VIDEO))
        (void)SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "ZA Recomp", message, NULL);
    if (message) {
        __android_log_print(ANDROID_LOG_WARN, ZA_LOG_TAG, "%s", message);
        free(message);
    }
    return NULL;
}
void plat_notice(const char *message) {
    if (!message) return;
    char *copy = strdup(message);
    if (!copy) return;
    pthread_t thread;
    if (pthread_create(&thread, NULL, notice_thread, copy) == 0)
        (void)pthread_detach(thread);
    else { __android_log_print(ANDROID_LOG_WARN, ZA_LOG_TAG, "%s", copy); free(copy); }
}

static void crash_handler(int sig, siginfo_t *info, void *context) {
    ucontext_t *machine = (ucontext_t *)context;
    uint64_t fault = info ? (uint64_t)(uintptr_t)info->si_addr : 0;
    uint64_t pc = 0, lr = 0;
#if defined(__aarch64__)
    if (machine) {
        pc = (uint64_t)machine->uc_mcontext.pc;
        lr = (uint64_t)machine->uc_mcontext.regs[30];
    }
#else
    (void)machine;
#endif
    if (rt_fault_watch && rt_fault_watch(fault, pc)) return;
    char reason[48];
    (void)snprintf(reason, sizeof(reason), "Android signal %d", sig);
    if (rt_fault_report) rt_fault_report(reason, fault, pc, lr);
    (void)signal(sig, SIG_DFL);
    (void)raise(sig);
    _exit(128 + sig);
}
void plat_crash_handler_install(void) {
    if (!g_alt_signal_stack) {
        g_alt_signal_stack = malloc(1u << 20);
        if (g_alt_signal_stack) {
            stack_t stack;
            memset(&stack, 0, sizeof(stack));
            stack.ss_sp = g_alt_signal_stack;
            stack.ss_size = 1u << 20;
            if (sigaltstack(&stack, NULL) != 0) {
                free(g_alt_signal_stack);
                g_alt_signal_stack = NULL;
                __android_log_print(ANDROID_LOG_ERROR, ZA_LOG_TAG,
                                    "sigaltstack failed: errno=%d", errno);
            }
        }
    }
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_sigaction = crash_handler;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
    (void)sigemptyset(&action.sa_mask);
    const int signals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT};
    for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); ++i)
        if (sigaction(signals[i], &action, NULL) != 0)
            __android_log_print(ANDROID_LOG_ERROR, ZA_LOG_TAG,
                                "sigaction(%d) failed: errno=%d", signals[i], errno);
}
bool plat_window_screen_hash(void *window, uint32_t *out) {
    (void)window; (void)out;
    return false; /* Desktop foreground-window capture has no Android equivalent. */
}
