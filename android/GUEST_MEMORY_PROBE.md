# Android guest-memory reservation probe

This diagnostic tests the non-Apple upstream guest window from the pinned
upstream runtime contract:

- Base: `0x7000000000`
- Size: `0x800000000` (32 GiB)
- Mapping: `PROT_NONE` plus `MAP_FIXED_NOREPLACE`
- One page is temporarily made read/write, written, read back, changed to
  read-only, then returned to `PROT_NONE`.
- The full reservation is released with `munmap` before the SDL loop starts.

The probe never uses `MAP_FIXED`, never overwrites an existing mapping, does
not touch game data, and does not keep the 32-GiB reservation alive. It records
the exact failing stage and errno in the combined diagnostic log.

## Device test

1. Install the newly built diagnostic APK.
2. Open **Launch SDL3 diagnostic**.
3. Wait for the SDL/Vulkan screen to appear, then return to the host screen.
4. Tap **Show combined SDL + Vulkan diagnostic log**.
5. Find lines beginning with `ZA_GUEST_MEM`.

Expected success marker:

```
ZA_GUEST_MEM result=PASS stages=reserve_32gib,commit_page,write_readback,protect_read,protect_none,release
```

A PASS validates only this address reservation/protection sequence in the
diagnostic process. It does not validate upstream guest memory tracking,
runtime initialization, generated code, or gameplay.
