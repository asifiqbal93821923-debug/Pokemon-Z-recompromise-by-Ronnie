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
    print("Applied Android/Bionic compatibility edits to pinned upstream runtime.")


if __name__ == "__main__":
    main()
