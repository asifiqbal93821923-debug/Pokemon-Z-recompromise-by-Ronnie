

### Build #47 — owner device retest (2026-11-06)

The owner supplied the full Build #47 `COPY FULL DIAGNOSTICS` report from OnePlus CPH2943 / Android 16 / arm64-v8a:

- Native JNI library loaded; 8 online CPUs; 4096-byte page size.
- SDL3 window/event loop and multitouch events passed.
- Vulkan Android surface, Mali-G615 MC2 (API 1.3.247), logical device, graphics/present queues, 2400×1080 six-image swapchain and first blue frame passed.
- Exact 32-GiB guest address reservation, one-page commit/write/readback, read-only protection, PROT_NONE and release passed.
- Corrected Build #47 feature gate was exercised. userfaultfd open and zero-feature query passed (mask `0x10ffe`), but `WP_ASYNC` and `WP_UNPOPULATED` remain unadvertised. Write tracking is therefore still unsupported on this device; use the upstream `-1` fallback.
- soft-dirty bit 55 was not observed on three pages; this is a device observation, not a universal AArch64 conclusion.
- PAGEMAP_SCAN generic ioctl returned EACCES / Permission denied. This is a procfs/SELinux policy block; support/backport status remains undetermined.
- MainActivity and SDL Activity lifecycle entries show orderly pause/stop/destroy and host Activity recreation/resume. No native fatal-error line appears in the supplied report; the reason for recreation is not proven.

This is device verification of the diagnostics only. It does not test the upstream runtime, production Android platform functions, generated game code or gameplay.

### Function-level platform API audit

Added `docs/ANDROID_PLATFORM_FUNCTION_CHECKLIST.md`, mapping every declaration in the pinned upstream `runtime/platform.h` to an Android implementation strategy and current evidence/status. This remains a plan; no production Android platform backend is claimed.


### Build #53 — Android platform API slice verified on device (2026-11-06)

The owner supplied the complete Build #53 diagnostics report from OnePlus CPH2943 / Android 16 / SDK 36 / arm64-v8a. The native Android platform slice was linked into the diagnostic JNI library for this isolated test (not into the upstream game runtime).

- `ZA_PLATFORM_API result=PASS`: exact 32-GiB guest reservation; commit/write/readback; read-only and PROT_NONE protection; one-page reset-to-zero; out-of-range protection rejection; monotonic clock; thread CPU clock; absolute monotonic sleep; CPU-affinity count (8); release.
- The same session reconfirmed the guest-memory probe PASS, Vulkan Android surface/device/queue/swapchain/first-frame PASS, SDL event loop and multitouch events.
- Write tracking remains unsupported on this device: userfaultfd opens, but required asynchronous write-protect features are not advertised; soft-dirty was not observed; PAGEMAP_SCAN generic ioctl is denied with EACCES. Keep `plat_written_pages()` and `plat_written_pages_peek()` returning -1.
- Host/SDL Activity lifecycle logs show pause/stop/destroy followed by host Activity recreation/resume. No native fatal-error line was supplied; the reason is not established.

**Scope:** this validates only the initial Android memory/time API slice in the diagnostic harness. It does not establish full upstream runtime integration, process/filesystem support, renderer compatibility, or playable gameplay.

## Android private storage and AssetManager bridge (diagnostic slice)

- Host Activity passes its app-private files/cache directories and Java AssetManager to the JNI library.
- Native probe creates separate za-data, za-save and za-cache directories, writes/reads/deletes sentinel files, and reads a project-authored text asset using NDK AAssetManager.
- This tests only the Java-to-native storage/resource bridge. It does not import game data, implement SAF, or integrate upstream runtime/paths.c.
- A SAF content:// URI must be read through Android ContentResolver and copied into app-private storage; it must not be passed directly to POSIX open().


### Build #59 — SAF document import bridge (CI build pending device verification)

- Added a single-document Android Storage Access Framework picker using `ACTION_OPEN_DOCUMENT` / `CATEGORY_OPENABLE`; no broad storage permission is requested.
- The selected `content://` URI is consumed only by Android `ContentResolver` / `InputStream`; it is never passed to native POSIX file APIs.
- Import streams in 1-MiB chunks into a temporary file under app-private `files/za-import/`, checks available space with a 16-MiB reserve, validates provider-reported size when available, computes SHA-256, fsyncs, then publishes the completed file by same-directory atomic rename (with same-directory rename fallback).
- Destination names are sanitized and prefixed with a random UUID. Failed/incomplete copies are removed; the URI permission is not persisted because the file is copied immediately.
- The full diagnostics clipboard report now includes the SAF import result, byte count, SHA-256, MIME type and app-private destination.
- This is an import transport probe only. It does not decrypt, unpack, interpret, or execute imported content; it does not integrate the upstream `runtime/paths.c` data-root policy or make the game playable. Large-file behavior still requires on-device testing with a non-sensitive test file and adequate free space.


### Build #61 — persist Android diagnostic summaries across Activity recreation

- MainActivity now restores the Android Storage Bridge and SAF import result strings from app-private SharedPreferences during `onCreate`.
- Storage probe success/failure and SAF cancellation are persisted when reported.
- SAF import completion is persisted from the worker thread before posting the UI update, so an Activity recreation after a long copy does not reset the completed import result to “Not run yet”.
- This stores only short diagnostic summaries (including the existing import byte count, SHA-256 and app-private path); it does not copy or modify imported content.
- Requires a fresh on-device test: relaunch/recreate the host Activity, then use COPY FULL DIAGNOSTICS and confirm both sections retain their prior results. Existing reports on the current installed APK are not retroactively migrated.


### Build #62 — durable diagnostic summaries and existing-import recovery

- Split SharedPreferences persistence for the Storage Bridge and SAF report so updating one result cannot overwrite the other with a stale Activity field.
- Both report strings are restored during MainActivity creation; completed SAF import results are persisted by the worker before any UI callback.
- Added “Verify existing imported file (no re-copy)”: scans app-private files/za-import, selects the most recently modified regular non-hidden completed file (excluding .part and symlinks), and recomputes byte count and SHA-256 on a background thread. This allows an APK update to reconstruct the prior import report without copying the multi-gigabyte source again.
- Existing imports are not automatically re-hashed at launch; the owner explicitly starts verification. The recovery scan currently chooses the newest eligible file if multiple imports exist.
- On-device verification is required for SharedPreferences restoration and large-file rehash timing. This remains a diagnostic/import utility, not game data parsing or runtime integration.


### Build #64 — side-by-side recovery package

- Changed the debug application ID suffix from `.debug` to `.recovery` and incremented version metadata. The resulting package is `org.ronnie.zarecomp.recovery`, separate from the existing `org.ronnie.zarecomp.debug` installation.
- This avoids an Android package/signature update conflict by installing as a second app; it does not replace or modify the old app or its private data.
- Android private app storage is sandboxed by package. The new recovery app cannot read the old app's `files/za-import` copy. To test this build, select the original NSP from the user's Google Drive provider through the SAF picker and import it again into the new app's private storage. Keep the original Drive file and old app until the new import has been independently verified.
- This remains an import/diagnostic utility only, not a playable game. CI debug signing remains ephemeral; this side-by-side package avoids relying on matching signing keys for this recovery test.

### Build #64 — owner device report (2026-11-06)

The owner supplied the Build #64 report from OnePlus CPH2943 / Android 16 / arm64-v8a:

- JNI native library loaded successfully; 8 online CPUs and 4096-byte page size reported.
- Local Downloads-folder SAF import passed: 4,325,966,911 bytes copied into the recovery package's private `files/za-import/` directory; SHA-256 `c494c0f9225370e6e39d4c6634b0be1cbc74af087bb87ef9b271594ce84f2521`. This matches the previously recorded Build #60 byte-copy checksum.
- A separate attempt through the Google Drive document provider failed with `FileNotFoundException` / provider message `Cello error 8`. This is recorded as a provider-specific import failure; it does not establish corruption of the source file.
- Android Storage Bridge was not run in this report. SDL/Vulkan/guest-memory diagnostics were also not run in this report.

**Scope:** successful local SAF import proves only byte copying and checksum integrity. It does not parse, decrypt, unpack, or execute the NSP, and it does not establish game compatibility or playable gameplay.

### Build #64 — SDL3 / Vulkan / Guest Memory phone report (2026-11-06)

Owner-supplied diagnostic log from OnePlus CPH2943, Android 16 / SDK 36 / arm64-v8a:

- Android platform diagnostic API slice: PASS (system info, exact 32 GiB guest reservation, commit/readback/protection/reset, range rejection, clocks/sleep/CPU count, filesystem operations, release).
- Guest memory probe: PASS for exact reservation at `0x7000000000`, page commit, write/readback, read-only and no-access protection, release.
- Vulkan: Android loader and instance created; Android surface created; Mali-G615 MC2 (API 1.3.247); logical device and graphics/present queues created; 2400x1080 swapchain with 6 images; first blue frame presented.
- SDL3 window, renderer, event loop and single-/multi-touch input events: PASS.
- SAF local Downloads import: PASS, 4,325,966,911 bytes, SHA-256 `c494c0f9225370e6e39d4c6634b0be1cbc74af087bb87ef9b271594ce84f2521`.
- Android Storage Bridge remains NOT RUN in this report.
- Write tracking limitations observed: required userfaultfd write-protect features are not advertised; generic PAGEMAP_SCAN ioctl returns EACCES / Permission denied. Do not report written-page tracking as supported; retain unsupported status until a safe Android-compatible implementation is validated.
- Executable path remains expected-unsupported pending Java/AssetManager bridge.

**Scope:** These are diagnostic-harness results, not a production game runtime. No NSP parsing/decryption, game asset loading, recompilation, or playable gameplay was demonstrated.
