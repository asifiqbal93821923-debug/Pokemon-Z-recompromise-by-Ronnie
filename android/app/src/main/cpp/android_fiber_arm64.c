#include "android_fiber_arm64.h"
#include <sched.h>
#include <sys/mman.h>
#include <stdbool.h>

static ZaAndroidFiberContext probe_main_context;
static ZaAndroidFiberContext probe_worker_context;
static volatile int probe_fiber_sequence;

void za_android_fiber_return_trap(void) {
    __builtin_trap();
}

static void probe_worker_entry(void) {
    probe_fiber_sequence = 1;
    za_android_fiber_context_swap(&probe_worker_context, &probe_main_context);
    probe_fiber_sequence = 3;
    za_android_fiber_context_swap(&probe_worker_context, &probe_main_context);
    for (;;) sched_yield();
}

bool za_android_fiber_context_probe(void) {
    const size_t stack_size = 64u * 1024u;
    void *stack = mmap(NULL, stack_size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (stack == MAP_FAILED) return false;
    probe_fiber_sequence = 0;
    za_android_fiber_context_init(&probe_worker_context,
                                 (char *)stack + stack_size,
                                 probe_worker_entry);
    za_android_fiber_context_swap(&probe_main_context, &probe_worker_context);
    if (probe_fiber_sequence != 1) {
        munmap(stack, stack_size);
        return false;
    }
    probe_fiber_sequence = 2;
    za_android_fiber_context_swap(&probe_main_context, &probe_worker_context);
    bool ok = probe_fiber_sequence == 3;
    munmap(stack, stack_size);
    return ok;
}
