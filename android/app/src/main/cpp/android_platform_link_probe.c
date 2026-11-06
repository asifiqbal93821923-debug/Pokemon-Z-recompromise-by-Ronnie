#include "platform.h"
#include <stddef.h>

typedef struct {
    bool (*reserve_guest)(uint64_t, uint64_t);
    void (*commit)(uint64_t, uint64_t);
    bool (*reset_guest)(uint64_t, uint64_t);
    int (*written_pages)(uint64_t, uint64_t, uint64_t *, int, uint64_t *);
    int (*written_pages_peek)(uint64_t, uint64_t);
    void (*write_tracking_off)(const char *);
    bool (*page_state)(uint64_t, char *, size_t);
    bool (*mem_usage)(uint64_t *, uint64_t *);
    void (*sample_focus)(void);
    int (*protect)(uint64_t, uint64_t, int);
    uint64_t (*now_ns)(void);
    void (*sleep_until_ns)(uint64_t);
    void (*thread_realtime)(uint64_t);
    int (*thread_realtime_check)(uint64_t);
    int (*cpu_count)(void);
    void (*thread_guest)(int);
    uint64_t (*thread_cpu_ns)(void);
    void (*init)(void);
    void (*system_info)(PlatSysInfo *);
    bool (*exe_path)(char *, size_t);
    void (*os_name)(char *, size_t);
    void (*exec_process)(char *const []);
    bool (*relaunch)(void);
    void (*terminate)(void);
    PlatProc (*spawn)(const char *const [], const char *, bool);
    int (*proc_wait)(PlatProc, bool);
    void (*proc_kill)(PlatProc);
    bool (*remove_tree)(const char *);
    bool (*is_link)(const char *);
    int (*open_file)(const char *, int);
    int (*rename_file)(const char *, const char *);
    bool (*onedrive_synced)(const char *);
    const char *(*symbol)(uint64_t, uint64_t *);
    bool (*module_name)(uint64_t, char *, size_t);
    void (*print_backtrace)(void);
    void (*fatal_notice)(void);
    void (*notice)(const char *);
    void (*crash_handler_install)(void);
    bool (*window_screen_hash)(void *, uint32_t *);
} ZaPlatformSymbols;

static volatile ZaPlatformSymbols symbols = {
    plat_reserve_guest, plat_commit, plat_reset_guest,
    plat_written_pages, plat_written_pages_peek, plat_write_tracking_off,
    plat_page_state, plat_mem_usage, plat_sample_focus, plat_protect,
    plat_now_ns, plat_sleep_until_ns, plat_thread_realtime,
    plat_thread_realtime_check, plat_cpu_count, plat_thread_guest,
    plat_thread_cpu_ns, plat_init, plat_system_info, plat_exe_path,
    plat_os_name, plat_exec, plat_relaunch, plat_terminate, plat_spawn,
    plat_proc_wait, plat_proc_kill, plat_remove_tree, plat_is_link,
    plat_open_file, plat_rename, plat_onedrive_synced, plat_symbol,
    plat_module_name, plat_print_backtrace, plat_fatal_notice, plat_notice,
    plat_crash_handler_install, plat_window_screen_hash
};

int za_android_platform_link_probe(void) {
    const volatile ZaPlatformSymbols *p = &symbols;
    int n = 0;
    n += p->reserve_guest != NULL; n += p->commit != NULL;
    n += p->reset_guest != NULL; n += p->written_pages != NULL;
    n += p->written_pages_peek != NULL; n += p->write_tracking_off != NULL;
    n += p->page_state != NULL; n += p->mem_usage != NULL;
    n += p->sample_focus != NULL; n += p->protect != NULL;
    n += p->now_ns != NULL; n += p->sleep_until_ns != NULL;
    n += p->thread_realtime != NULL; n += p->thread_realtime_check != NULL;
    n += p->cpu_count != NULL; n += p->thread_guest != NULL;
    n += p->thread_cpu_ns != NULL; n += p->init != NULL;
    n += p->system_info != NULL; n += p->exe_path != NULL;
    n += p->os_name != NULL; n += p->exec_process != NULL;
    n += p->relaunch != NULL; n += p->terminate != NULL;
    n += p->spawn != NULL; n += p->proc_wait != NULL;
    n += p->proc_kill != NULL; n += p->remove_tree != NULL;
    n += p->is_link != NULL; n += p->open_file != NULL;
    n += p->rename_file != NULL; n += p->onedrive_synced != NULL;
    n += p->symbol != NULL; n += p->module_name != NULL;
    n += p->print_backtrace != NULL; n += p->fatal_notice != NULL;
    n += p->notice != NULL; n += p->crash_handler_install != NULL;
    n += p->window_screen_hash != NULL;
    return n;
}
