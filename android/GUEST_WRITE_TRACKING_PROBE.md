# Android guest-page write-tracking probe

This diagnostic checks the Linux mechanism used by the upstream runtime/platform/linux.c. It does not load game code or alter the upstream runtime.

## What it tests

1. Reserves the same 32-GiB guest virtual address window at 0x7000000000, using MAP_FIXED_NOREPLACE so an existing mapping is never overwritten.
2. Opens userfaultfd with UFFD_USER_MODE_ONLY.
3. Negotiates UFFD_FEATURE_WP_ASYNC and UFFD_FEATURE_WP_UNPOPULATED.
4. Registers the full guest window in UFFDIO_REGISTER_MODE_WP.
5. Opens /proc/self/pagemap.
6. Write-protects one test page, writes a marker, and asks PAGEMAP_SCAN to report and re-protect the written page.
7. Calls the scan again to verify that the write-tracking reset cleared the reported state.
8. Unregisters and releases the mapping and file descriptors on every exit path.

The probe logs each stage to the existing app-private combined diagnostic log. Kernel denial, missing kernel features, unsupported ioctls, or inaccessible pagemap are reported as unsupported/failed, not as a fatal app error.

## Expected outcomes

- ZA_WRITE_TRACK result=PASS: the Android kernel exposes the required asynchronous userfaultfd write-protection and PAGEMAP_SCAN behavior for the tested mapping.
- ZA_WRITE_TRACK result=UNSUPPORTED_OR_FAILED: inspect the preceding stage and errno. This is a compatibility result, not a crash.
- ZA_WRITE_TRACK result=UNAVAILABLE: the Android NDK headers do not expose the required syscall number at build time.

A PASS is a prerequisite for directly adapting upstream's Linux write-tracking implementation. It is not proof that all guest-memory behavior or the game runtime works on Android.
