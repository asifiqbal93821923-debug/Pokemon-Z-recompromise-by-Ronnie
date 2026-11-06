

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


### Build #65 — automatic Android Storage Bridge execution (2026-11-06)

- The recovery host now runs the existing small Android private-storage + NDK AssetManager probe automatically after MainActivity is created; the user no longer needs to find or tap the probe button.
- The probe runs on a background thread, stores its result in app-private SharedPreferences, and leaves the manual retry button available.
- It checks creation of app-private data/save/cache directories, sentinel file write/read/delete round-trips, and reading the project-authored APK asset through AAssetManager.
- This changes the diagnostic from “Not run yet” to an actual device result on next launch. PASS is not assumed until the owner supplies the resulting report.
- Write tracking remains a separate kernel capability limitation: this Android 6.1 device does not advertise the required userfaultfd write-protect features and procfs PAGEMAP_SCAN is denied. The app must not falsely mark it as running or use an unsafe signal-handler substitute. Keep written-page tracking unsupported (-1) until a safe, validated backend exists.
- No game runtime, NSP parsing/decryption, or gameplay integration is added by this diagnostic change.


### Build #67 — owner device report: Android Storage Bridge PASS (2026-11-06)

Owner supplied the Build #67 report from OnePlus CPH2943 / Android 16 (SDK 36) / arm64-v8a:

- `ANDROID_STORAGE_BRIDGE result=PASS`.
- Private directories: PASS.
- File write/read round-trip: PASS.
- APK AssetManager read: PASS.
- Native JNI library loaded; aarch64, kernel 6.1.157-android14, 8 online CPUs, 4096-byte pages.
- SAF import in the separate `.storageprobe` package was not run. The previously verified large local Downloads import remains in the separate `.recovery` package; do not uninstall either package or assume cross-package private storage access.
- SDL/Vulkan/guest-memory log was not present because the SDL3 diagnostic Activity had not yet been launched from this package. Next device action: tap “Launch SDL3 diagnostic”, allow it to finish, then “COPY FULL DIAGNOSTICS” and provide the report.

**Scope:** Storage bridge PASS verifies private directory/file operations and reading a project-authored APK asset. It does not establish NSP parsing/decryption, game asset loading, recompilation, production runtime, or playable gameplay. Write tracking remains blocked pending a safe Android-compatible implementation; do not mark it PASS based on the storage result.


### Build #67 — owner device report: SDL3 / Vulkan / guest memory PASS (2026-11-06)

Owner supplied a second Build #67 report from OnePlus CPH2943 / Android 16 (SDK 36) / arm64-v8a:

- Android Storage Bridge: PASS (private directories, file round-trip, APK AssetManager read).
- SDL3: native window, renderer, event loop, Activity lifecycle, single-touch and multi-touch events all observed.
- Vulkan: Android loader and instance created; Android surface created; Mali-G615 MC2 detected (vendor 0x13b5, device 0xb8a31030, Vulkan API 1.3.247); logical device and graphics/present queues created; 2400x1080 swapchain, format 37, six images; first blue frame presented.
- Android platform API diagnostic: PASS for system info, 32 GiB reservation, commit/write/read/protection, reset, outside-range rejection, monotonic/thread CPU timing, absolute sleep, CPU count, filesystem operations, and release. Executable path is explicitly UNSUPPORTED_EXPECTED because APK assets require the Java/AssetManager bridge; the separate Storage Bridge confirms AssetManager asset reads, but a production guest executable path adapter is not implemented.
- Guest memory: PASS for exact 32 GiB reservation at 0x7000000000, commit, write/readback, read-only/no-access protection and release.
- Write tracking: capability report completed, but actual written-page tracking remains unsupported. userfaultfd open and UFFDIO_API query pass; required WP features PAGEFAULT_FLAG_WP, WP_HUGETLBFS_SHMEM, WP_UNPOPULATED and WP_ASYNC are not advertised. PAGEMAP_SCAN generic ioctl returns EACCES/Permission denied; stock 6.1 support is not expected and vendor backport is unverified. ARM64 soft-dirty bit 55 was not observed on three test pages. Do not claim tracking support or use an unsafe signal-handler fallback.
- SAF import in .storageprobe: NOT RUN. The previously verified 4,325,966,911-byte local Downloads import with SHA-256 c494c0f9225370e6e39d4c6634b0e1cbc74af087bb87ef9b271594ce84f2521 remains in the separate .recovery package; package-private files are not shared between apps.
- Activity lifecycle: create/start/resume, pause/stop/destroy and a subsequent host Activity recreation were recorded. SDL diagnostic Activity ended normally.
- No game runtime, NSP parsing/decryption, game asset loading, recompilation, or playable gameplay was demonstrated.

**Next technical work:** implement a dedicated Android executable-path / asset access adapter only after reviewing upstream platform contracts; investigate a safe software dirty-page tracking design that can be validated on Android ARM64. Keep unsupported status until correctness is demonstrated. Do not ask the owner to re-import the large file unless a later test specifically requires it.


### Build #68 — safe Android write-tracking fallback contract

- Device evidence showed Android 16 / kernel 6.1 lacks the required userfaultfd WP features, PAGEMAP_SCAN is denied, and ARM64 soft-dirty bit 55 is not a supported substitute.
- Added an explicit Android platform comment: `plat_written_pages()` and `plat_written_pages_peek()` return the upstream contract's `-1` sentinel when tracking is unavailable; callers must fall back to conservative full-range copies.
- Extended the Android platform API diagnostic to exercise both functions against a committed guest page and require `-1`, logging `ZA_WRITE_TRACK fallback result=PASS mode=NO_TRACKER_FULL_RANGE_COPY_REQUIRED` only when the contract is met.
- This resolves the unsafe assumption/blocked-kernel dependency at the platform API boundary. It does NOT implement dirty-page tracking, and the real upstream renderer still must be integrated and verified to honor the `-1` fallback before this can be considered runtime-complete.

### Build #69 — owner device report: Android write-tracking fallback PASS (2026-11-06)

Owner supplied the full Build #69 diagnostics report from OnePlus CPH2943 / Android 16 (SDK 36) / arm64-v8a:

- `ANDROID_STORAGE_BRIDGE result=PASS`: private directories, file round-trip and NDK AssetManager read passed.
- `ZA_PLATFORM_API result=PASS`: system info, exact 32-GiB reservation, commit/write/readback, read-only and PROT_NONE protection, reset-to-zero, out-of-range rejection, monotonic/thread CPU clocks, absolute sleep, CPU count, filesystem operations and release passed. APK executable path remains explicitly UNSUPPORTED_EXPECTED; the Java/AssetManager bridge is not yet wired as a production guest executable path.
- `ZA_GUEST_MEM result=PASS`: exact reservation at 0x7000000000, one-page commit/write/readback/protection and release passed.
- `ZA_WRITE_TRACK fallback result=PASS mode=NO_TRACKER_FULL_RANGE_COPY_REQUIRED written_pages=-1 peek=-1`: both Android platform API functions returned the upstream -1 sentinel as required. This confirms safe capability reporting only; it does not test the real Vulkan renderer's AUTO fallback.
- Capability evidence remains unchanged: userfaultfd opens and UFFDIO_API query succeeds (mask 0x10ffe), but required WP features are not advertised; PAGEMAP_SCAN generic ioctl is denied with EACCES; ARM64 soft-dirty bit 55 was not observed on the three test pages. Do not mark dirty-page tracking supported.
- SDL3 window/renderer/event loop and single-/multi-touch events passed. Vulkan Android surface, Mali-G615 MC2 device (API 1.3.247), logical device, graphics/present queues, 2400x1080 six-image swapchain and first blue frame passed.
- SAF import in this `.writefallback` package was not run. The previously verified 4,325,966,911-byte import remains in the separate `.recovery` package; private storage is not shared. No re-import is needed for this diagnostic result.

**Scope:** Build #69 is still a diagnostic harness, not the upstream game runtime. No NSP parsing/decryption, game asset loading, recompilation, or gameplay was demonstrated.

**Renderer source audit:** At pinned upstream commit `dfb47e9d8130d6ecde1c0afa1010fb1a102e50f6`, `runtime/nvn/vulkan/exec.c` checks `pages_cpu_written(HEAP_BASE, 4096, ...)`. In AUTO mode, when no page tracking is available, it sets GPU overlap OFF and logs that it is using the normal full fence wait. An explicit GPU_OVERLAP ON setting or `LARECOMP_GPU_OVERLAP=1` can override AUTO; Android integration must not force that override without validating the alternate watch path. The renderer behavior itself has not yet been exercised on Android.


### Build #72 — Android platform backend linked (2026-11-06)

- Commit: 4ff28e326597798f2ba329fbf6066e6a4054054a
- Workflow run: 42217357351 — completed successfully against the exact commit.
- The Android NDK target now compiles and links android_platform.c and android_platform_link_probe.c into the SDL native shared library; it no longer treats the backend as compile-only.
- The link probe references 39 common upstream platform API symbols applicable to Android.
- The native diagnostic now checks that XDG_DATA_HOME is an absolute writable app-private path after plat_init().
- Build success is source/CI evidence only. The XDG path assertion still needs the owner's on-device report.
- Full Phase 1 remains open: the repository still lacks the full upstream runtime source tree and the Android target still has explicit unsupported process operations. See ANDROID_RUNTIME_PHASE1.md.


### Android upstream core compile and path-manager link gate (2026-11-06)

- Runtime root C/C++ + HLE + services sources now compile into the Android ARM64 static compile-check archive.
- The Android diagnostic shared library now links that archive and calls upstream rt_paths_init()/rt_data_root(), forcing runtime/paths.c into the APK link.
- The on-device probe checks the selected path is the writable app-private XDG_DATA_HOME/zarecomp directory.
- This is not yet the complete runtime entry point or game boot. UI/renderer backends, generated game library, Activity lifecycle bridge, and helper-process redesign remain.


### Bionic path-buffer compatibility fixes

The Android-only CI patch now adjusts the POSIX realpath output buffers in upstream gamebuild.c and main.c to PATH_MAX, leaving Windows-specific branches untouched. These fixes came from actual Android NDK diagnostics; the next CI run verifies the complete core compile set again.


### Multimedia dependency boundary

The upstream movie HLE requires FFmpeg headers/libraries absent from the Android target. It is explicitly excluded from the Phase 1 core compile-check until real Android ARM64 FFmpeg libraries are integrated; no placeholder movie implementation is supplied.
