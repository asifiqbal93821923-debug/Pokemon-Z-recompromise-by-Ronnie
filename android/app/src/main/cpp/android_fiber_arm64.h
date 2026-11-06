#ifndef ZA_ANDROID_FIBER_ARM64_H
#define ZA_ANDROID_FIBER_ARM64_H

#include <stdint.h>
#include <string.h>

#if !defined(__ANDROID__) || !defined(__aarch64__)
#error "Android ARM64 fiber context backend included for a different target"
#endif

typedef struct {
    uint64_t x19_x30[12];
    uint64_t sp;
    uint64_t d8_d15[8];
    uint64_t fpcr;
    uint64_t fpsr;
} ZaAndroidFiberContext;

void za_android_fiber_context_swap(ZaAndroidFiberContext *from,
                                  const ZaAndroidFiberContext *to);
void za_android_fiber_context_bootstrap(void);
void za_android_fiber_return_trap(void);

static inline void za_android_fiber_context_init(ZaAndroidFiberContext *context,
                                                void *stack_top,
                                                void (*entry)(void)) {
    memset(context, 0, sizeof(*context));
    context->sp = (uint64_t)(uintptr_t)stack_top & ~((uint64_t)15);
    context->x19_x30[0] = (uint64_t)(uintptr_t)entry;
    context->x19_x30[11] = (uint64_t)(uintptr_t)za_android_fiber_context_bootstrap;
}

#endif
