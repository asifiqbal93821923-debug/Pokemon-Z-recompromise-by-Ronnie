# Upstream Source Index

This is a navigation and audit index, not a copy of the upstream source.

## Primary upstream repository

- URL: https://github.com/Hexveil404/PokeZ-ARecomp
- Research branch: `pokemon-za`
- Audited upstream commit: `dfb47e9d8130d6ecde1c0afa1010fb1a102e50f6` (2026-11-05)
- Upstream README: https://github.com/Hexveil404/PokeZ-ARecomp/blob/dfb47e9d8130d6ecde1c0afa1010fb1a102e50f6/README.md
- Upstream license: MIT for LAS Recomp's own code; third-party components keep their own licenses. The license does not cover Nintendo game properties or game files.
- Re-check the branch head and license before importing or adapting source; the branch may move.

## Confirmed source contracts at the audited commit

| Upstream file | Confirmed detail | Android port implication |
|---|---|---|
| `Makefile` | Platform selection is Windows/Linux/macOS; runtime selects `runtime/platform/$(PLATFORM).c` | Add a deliberate Android build path; do not set `PLATFORM=android` without full flags/dependency integration |
| `runtime/platform.h` | Broad platform API: guest VM, timing, scheduling, process, child process, filesystem and diagnostics | Android backend must satisfy or deliberately redesign each call site |
| `runtime/rt.h` | Non-Apple guest window is `0x7000000000` to `0x7800000000` (32 GiB); Z-A reserves room for SDK virtual address memory | Test exact-range reservation/protection in the Android app process before runtime integration |
| `runtime/platform/linux.c` | Uses `MAP_FIXED_NOREPLACE`, `MAP_FIXED`, optional userfaultfd/PAGEMAP_SCAN write tracking, procfs and process/scheduling APIs | Android/Bionic and SELinux behavior must be verified; do not copy Linux backend wholesale |
| `runtime/main.c` | Constructor reserves guest address space before main; runtime has process re-exec and helper assumptions | Android needs an early, lifecycle-aware native entry path and no desktop re-exec assumption |
| `runtime/paths.h` / `runtime/paths.c` | Data root includes extracted data, saves, settings, caches and imported fonts | Use app-private storage; SAF content URIs require a descriptor/stream bridge |
| `runtime/nvn/vulkan/device.c` | SDL Vulkan extension/surface and swapchain path are part of the actual renderer | Probe Vulkan success does not prove renderer/device-memory integration |

## Areas to inspect

- `Makefile` — platform selection, native build and dependencies.
- `runtime/platform.h` — host platform API.
- `runtime/platform/linux.c` — Linux reference implementation.
- `runtime/rt.h` — guest address window and runtime constants.
- `runtime/main.c` — constructor-time guest reservation and runtime startup.
- `runtime/paths.h`, `runtime/paths.c` — data-root and resource-path contract.
- `runtime/nvn/vulkan/` — Vulkan graphics backend.
- SDL3 integration — Activity, events, pause/resume and touch.
- `tools/za/` and `tools/` — host-side import/recompiler tools; keep user-owned game data and generated code private.

## Reuse policy

- Prefer a pinned upstream submodule or a clearly tracked patch series over copying the entire upstream repository into this workspace.
- Preserve upstream MIT and third-party license notices when adapting code.
- Maintain a dependency/license inventory before bundling libraries.
- Do not copy or commit Nintendo game files, extracted data, generated game code, fonts, saves, keys or firmware.
- Do not import unrelated game projects as though they were Pokémon Z-A source.
