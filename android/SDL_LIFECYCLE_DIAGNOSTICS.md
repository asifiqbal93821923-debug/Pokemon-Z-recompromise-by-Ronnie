# Android SDL3 Lifecycle Diagnostics

Build 25 adds durable Java Activity lifecycle breadcrumbs.

- SDL Activity create/start/resume/pause/stop/destroy and window-focus events are appended to the app-private file `za_sdl_diagnostics.log`.
- The Java host screen reads that file using **Show SDL diagnostic log** after returning from SDL.
- Both processes use the same application-private files directory.
- Native SDL startup logs remain available through Android/SDL logging; this file records Activity lifecycle events.
- CI success verifies compilation and packaging only, not runtime stability on a physical device.

This remains a diagnostic shell, not the Pokémon Z-A game.
