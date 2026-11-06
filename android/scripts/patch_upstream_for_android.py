#!/usr/bin/env python3
"""Apply narrowly scoped, reproducible Android/Bionic compatibility edits."""
from __future__ import annotations

import sys
from pathlib import Path


def replace_once(path: Path, old: str, new: str) -> None:
    source = path.read_text(encoding="utf-8")
    count = source.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one match, found {count}: {old!r}")
    path.write_text(source.replace(old, new, 1), encoding="utf-8")


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: patch_upstream_for_android.py UPSTREAM_ROOT")
    root = Path(sys.argv[1]).resolve()
    platform = root / "runtime/platform.h"
    gamebuild = root / "runtime/gamebuild.c"
    entry = root / "runtime/main.c"
    for file in (platform, gamebuild, entry):
        if not file.is_file():
            raise SystemExit(f"missing pinned upstream source file: {file}")

    replace_once(
        platform,
        '#elif defined(__linux__)\n#include "platform/linux_compat.h"',
        '#elif defined(__linux__) && !defined(__ANDROID__)\n#include "platform/linux_compat.h"',
    )
    replace_once(
        gamebuild,
        '#include <stdio.h>\n',
        '#include <stdio.h>\n#ifndef _WIN32\n#include <limits.h>\n#endif\n',
    )
    replace_once(
        gamebuild,
        '        char zc[1200], rel[1200];',
        '#ifdef _WIN32\n        char zc[1200], rel[1200];\n#else\n        char zc[PATH_MAX], rel[PATH_MAX];\n#endif',
    )
    replace_once(
        entry,
        '#include <string.h>\n',
        '#include <string.h>\n#ifndef _WIN32\n#include <limits.h>\n#endif\n',
    )
    replace_once(
        entry,
        '    static char arg_root[1024];',
        '#ifdef _WIN32\n    static char arg_root[1024];\n#else\n    static char arg_root[PATH_MAX];\n#endif',
    )

    checks = {
        platform: '#elif defined(__linux__) && !defined(__ANDROID__)',
        gamebuild: 'char zc[PATH_MAX], rel[PATH_MAX];',
        entry: 'static char arg_root[PATH_MAX];',
    }
    for file, token in checks.items():
        if token not in file.read_text(encoding="utf-8"):
            raise SystemExit(f"post-edit verification failed: {file} missing {token!r}")
    fiber = root / "runtime/hle/fiber.c"
    fiber_source = fiber.read_text(encoding="utf-8")

    def replace_fiber(old: str, new: str, label: str) -> None:
        nonlocal fiber_source
        count = fiber_source.count(old)
        if count != 1:
            raise SystemExit(f"{fiber}: expected one {label} anchor, found {count}")
        fiber_source = fiber_source.replace(old, new, 1)

    replace_fiber(
        '#include "../hle.h"\n',
        '#include "../hle.h"\n#ifdef __ANDROID__\n#include "android_fiber_arm64.h"\n#if !defined(__aarch64__)\n#error "Android fibers currently require arm64-v8a"\n#endif\n#endif\n',
        "Android fiber header",
    )
    replace_fiber(
        '#else\n    ucontext_t uc;\n    void *stack;\n#endif',
        '#elif defined(__ANDROID__)\n    ZaAndroidFiberContext ctx;\n    void *stack;\n#else\n    ucontext_t uc;\n    void *stack;\n#endif',
        "fiber context field",
    )
    replace_fiber(
        '#else\nstatic __thread ucontext_t t_tramp_uc, t_dummy_uc;\nstatic __thread void *t_tramp_stack;\n#endif',
        '#elif defined(__ANDROID__)\nstatic __thread ZaAndroidFiberContext t_tramp_ctx;\nstatic __thread void *t_tramp_stack;\n#else\nstatic __thread ucontext_t t_tramp_uc, t_dummy_uc;\nstatic __thread void *t_tramp_stack;\n#endif',
        "thread trampoline context",
    )
    replace_fiber(
        '#else\nstatic void tramp_main(void) {\n    for (;;) { tramp_step(); swapcontext(&t_dummy_uc, &t_to->uc); }\n}\n#endif',
        '#elif defined(__ANDROID__)\nstatic void tramp_main(void) {\n    for (;;) { tramp_step(); za_android_fiber_context_swap(&t_tramp_ctx, &t_to->ctx); }\n}\n#else\nstatic void tramp_main(void) {\n    for (;;) { tramp_step(); swapcontext(&t_dummy_uc, &t_to->uc); }\n}\n#endif',
        "Android trampoline",
    )
    replace_fiber(
        '#else\n    if (fresh) {\n        if (!dst->stack) {\n            dst->stack = mmap(NULL, CO_STACK, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);\n            if (dst->stack == MAP_FAILED) FATAL("[fiber] no host stack\\n");\n        }\n        getcontext(&dst->uc);\n        dst->uc.uc_stack.ss_sp = dst->stack;\n        dst->uc.uc_stack.ss_size = CO_STACK;\n        dst->uc.uc_link = NULL;\n        makecontext(&dst->uc, co_main, 0);\n        dst->started = 1;\n    }\n#endif',
        '#elif defined(__ANDROID__)\n    if (fresh) {\n        if (!dst->stack) {\n            dst->stack = mmap(NULL, CO_STACK, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);\n            if (dst->stack == MAP_FAILED) { dst->stack = NULL; FATAL("[fiber] no Android host stack\\\\n"); }\n        }\n        za_android_fiber_context_init(&dst->ctx, (char *)dst->stack + CO_STACK, co_main);\n        dst->started = 1;\n    }\n#else\n    if (fresh) {\n        if (!dst->stack) {\n            dst->stack = mmap(NULL, CO_STACK, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);\n            if (dst->stack == MAP_FAILED) FATAL("[fiber] no host stack\\\\n");\n        }\n        getcontext(&dst->uc);\n        dst->uc.uc_stack.ss_sp = dst->stack;\n        dst->uc.uc_stack.ss_size = CO_STACK;\n        dst->uc.uc_link = NULL;\n        makecontext(&dst->uc, co_main, 0);\n        dst->started = 1;\n    }\n#endif',
        "fresh fiber setup",
    )
    replace_fiber(
        '#else\n    if (!t_tramp_stack) {\n        t_tramp_stack = mmap(NULL, 1 << 16, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);\n        if (t_tramp_stack == MAP_FAILED) FATAL("[fiber] no trampoline stack\\n");\n        getcontext(&t_tramp_uc);\n        t_tramp_uc.uc_stack.ss_sp = t_tramp_stack;\n        t_tramp_uc.uc_stack.ss_size = 1 << 16;\n        t_tramp_uc.uc_link = NULL;\n        makecontext(&t_tramp_uc, tramp_main, 0);\n    }\n    swapcontext(&me->uc, &t_tramp_uc);\n#endif',
        '#elif defined(__ANDROID__)\n    if (!t_tramp_stack) {\n        t_tramp_stack = mmap(NULL, 1 << 16, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);\n        if (t_tramp_stack == MAP_FAILED) { t_tramp_stack = NULL; FATAL("[fiber] no Android trampoline stack\\\\n"); }\n        za_android_fiber_context_init(&t_tramp_ctx, (char *)t_tramp_stack + (1 << 16), tramp_main);\n    }\n    za_android_fiber_context_swap(&me->ctx, &t_tramp_ctx);\n#else\n    if (!t_tramp_stack) {\n        t_tramp_stack = mmap(NULL, 1 << 16, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);\n        if (t_tramp_stack == MAP_FAILED) FATAL("[fiber] no trampoline stack\\\\n");\n        getcontext(&t_tramp_uc);\n        t_tramp_uc.uc_stack.ss_sp = t_tramp_stack;\n        t_tramp_uc.uc_stack.ss_size = 1 << 16;\n        t_tramp_uc.uc_link = NULL;\n        makecontext(&t_tramp_uc, tramp_main, 0);\n    }\n    swapcontext(&me->uc, &t_tramp_uc);\n#endif',
        "Android trampoline switch",
    )
    fiber.write_text(fiber_source, encoding="utf-8")
    print("Applied Android/Bionic platform and arm64 fiber compatibility edits.")



if __name__ == "__main__":
    main()
