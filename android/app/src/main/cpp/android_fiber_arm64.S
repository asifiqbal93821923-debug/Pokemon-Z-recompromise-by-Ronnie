.text
.align 2
.global za_android_fiber_context_swap
.type za_android_fiber_context_swap, %function
za_android_fiber_context_swap:
    stp x19, x20, [x0, #0]
    stp x21, x22, [x0, #16]
    stp x23, x24, [x0, #32]
    stp x25, x26, [x0, #48]
    stp x27, x28, [x0, #64]
    stp x29, x30, [x0, #80]
    mov x9, sp
    str x9, [x0, #96]
    stp d8, d9, [x0, #104]
    stp d10, d11, [x0, #120]
    stp d12, d13, [x0, #136]
    stp d14, d15, [x0, #152]
    mrs x9, fpcr
    str x9, [x0, #168]
    mrs x9, fpsr
    str x9, [x0, #176]

    ldp x19, x20, [x1, #0]
    ldp x21, x22, [x1, #16]
    ldp x23, x24, [x1, #32]
    ldp x25, x26, [x1, #48]
    ldp x27, x28, [x1, #64]
    ldp x29, x30, [x1, #80]
    ldp d8, d9, [x1, #104]
    ldp d10, d11, [x1, #120]
    ldp d12, d13, [x1, #136]
    ldp d14, d15, [x1, #152]
    ldr x9, [x1, #168]
    msr fpcr, x9
    ldr x9, [x1, #176]
    msr fpsr, x9
    ldr x9, [x1, #96]
    mov sp, x9
    ret
.size za_android_fiber_context_swap, .-za_android_fiber_context_swap

.global za_android_fiber_context_bootstrap
.type za_android_fiber_context_bootstrap, %function
za_android_fiber_context_bootstrap:
    mov x0, x19
    blr x0
    bl za_android_fiber_return_trap
    brk #0
.size za_android_fiber_context_bootstrap, .-za_android_fiber_context_bootstrap
