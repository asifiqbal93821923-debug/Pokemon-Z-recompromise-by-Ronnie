# Upstream Source Index

This is a navigation and audit index, not a copy of the upstream source.

## Primary upstream repository

- URL: https://github.com/Hexveil404/PokeZ-ARecomp
- Research branch: `pokemon-za`
- Upstream README: https://github.com/Hexveil404/PokeZ-ARecomp/blob/pokemon-za/README.md
- Upstream contributor/build notes: https://github.com/Hexveil404/PokeZ-ARecomp/blob/pokemon-za/CONTRIBUTING.md

## Areas to inspect

| Upstream area | Purpose | Android port questions |
|---|---|---|
| `Makefile` | Platform selection and native build | Add Android/NDK/CMake without breaking desktop builds |
| `runtime/platform.h` | Host platform abstraction | Which functions need Android-specific implementations? |
| `runtime/platform/linux.c` | Linux host implementation | Which APIs are absent, restricted or unsuitable on Android? |
| `runtime/main.c` | Runtime entry point and startup | SDL Activity lifecycle, app-private paths, restart/helper assumptions |
| `runtime/nvn/vulkan/` | Vulkan graphics backend | Android surface, swapchain recreation and supported features |
| SDL3 integration | Window, events and controller/input handling | Android Activity, touch events, pause/resume |
| `tools/za/` | Z-A-specific setup/import tools | Desktop subprocess and .NET dependencies; defer Android importer |
| `tools/` | Recompiler and game-code generation | Host toolchain availability and generated output must remain private |

Paths above are an initial inspection map; verify exact current contents against the upstream branch before editing.

## Reuse policy

- Prefer a documented upstream remote, submodule or clearly tracked patch series over copying the entire upstream repository into this workspace.
- Preserve MIT and third-party license notices when adapting code.
- Maintain a dependency/license inventory before bundling libraries.
- Do not copy or commit Nintendo game files, extracted data, generated game code, fonts, saves, keys or firmware.
- Do not import unrelated LAS Recomp or other game projects as though they were Pokémon Z-A source.
