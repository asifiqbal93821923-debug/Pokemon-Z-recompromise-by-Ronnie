# Android SDL3 Touch Test — Device Checklist

This APK is a native SDL3 Android shell test, not a playable Pokémon Z-A port.

## Install
1. Download the debug APK from the GitHub Actions artifact or the ChatGPT-provided APK link.
2. On the Android phone, allow installation from the file manager/browser if Android asks.
3. Install and launch **Pokémon Z-A Android Test**. Keep the phone in landscape orientation.

## Expected screen feedback
- Dark blue: no finger is currently held.
- Green: one finger is held.
- Amber: two or more fingers are held simultaneously.

## Test sequence
1. Tap and release once. The screen should change to green while held, then dark blue.
2. Hold one finger and drag it. It should remain green.
3. While holding the first finger, place a second finger elsewhere. The screen should turn amber.
4. Lift only one finger. It should return to green while the other finger remains down.
5. Lift the final finger. It should return to dark blue.
6. Rotate is intentionally locked to landscape; background/foreground behavior is managed by SDLActivity and should be checked by leaving and reopening the app.

## Report back
Tell us whether it installs, opens, and whether the one-finger / two-finger color sequence works. A screen recording is useful if any step fails.

## Current limits
- No game assets, ROM, keys, game code, emulator, Vulkan game renderer, movement controls, or camera controls are included.
- This test validates APK packaging, SDL3 Android Activity/native loading, basic rendering, and SDL touch event delivery only.
