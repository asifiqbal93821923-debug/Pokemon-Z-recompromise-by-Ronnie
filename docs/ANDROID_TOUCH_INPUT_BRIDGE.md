# Android touch-input bridge (prototype)

This module translates SDL3 finger events into game-neutral input events. It is designed to sit between SDL3 and a future game-input adapter; it does not emulate or modify Nintendo game code.

## Provisional invisible zones

- Left 42%: movement. Drag displacement from touch-down is normalized to -1..1.
- Right side above the bottom action strip: camera drag. Per-event delta is scaled by camera sensitivity.
- Bottom-right strip: two generic action channels with press/release events.
- Up to 16 fingers are tracked independently.

The two action channels are placeholders. Do not map them to jump, attack, interact, or any named game action until the actual Z-A input mapping is verified.

## Integration

Include `za_android_input.h`, call `za_android_input_init`, and pass SDL events to `za_android_input_handle_event`. The callback receives a game-neutral event with type, phase, finger ID, and normalized values. The eventual game adapter must aggregate concurrent streams by finger ID and bind actions to verified game controls.

Call `za_android_input_reset` on app pause/focus loss to release held inputs.

## Limitations

- Compiled as a standalone static library, but not yet wired to the recompiler/runtime.
- Screen zones and sensitivity are provisional and need device calibration and settings.
- Zones are invisible; this module draws no UI.
- No claims are made about device touch latency, GPU performance, or game compatibility.
