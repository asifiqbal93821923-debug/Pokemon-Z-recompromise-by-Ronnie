# Android Platform Function Checklist

Audited against pinned upstream `runtime/platform.h` at
`Hexveil404/PokeZ-ARecomp@dfb47e9d8130d6ecde1c0afa1010fb1a102e50f6`.

This is a function-by-function implementation plan. It is not an implementation or runtime integration claim.

## Guest memory

| API | Android implementation plan | Initial status |
|---|---|---|
| `plat_reserve_guest(lo, hi)` | Bionic `mmap` with exact-address, no-replace semantics; verify full range and alignment | Isolated 32-GiB reservation probe PASS; production API not implemented |
| `plat_commit(addr, size)` | Validate range/alignment; preserve upstream backing/commit semantics | One-page smoke test PASS; production API not implemented |
| `plat_reset_guest(lo, hi)` | Safe decommit/reset semantics while retaining reservation; requires source-level parity review | Not implemented |
| `plat_written_pages(...)` | Return `-1` unless exact safe tracking semantics are available | Unsupported on tested device |
| `plat_mem_usage(...)` | Use supported Android process metrics only; otherwise false | Not implemented |
| `plat_sample_focus()` | No-op on Android unless profiling requires a supported implementation | Not implemented |
| `plat_write_tracking_off(why)` | Linux-only API; no Android declaration unless upstream contract changes | Not applicable |
| `plat_written_pages_peek(...)` | Return `-1` while tracking unavailable | Not implemented |
| `plat_page_state(...)` | Return false unless page residency/state can be reported accurately | Not implemented |
| `plat_protect(addr,size,prot)` | Bionic `mprotect`; validate guest range, page alignment and enum mapping | One-page protection smoke test PASS; production API not implemented |

## Time and scheduling

| API | Android implementation plan | Initial status |
|---|---|---|
| `plat_now_ns()` | `clock_gettime(CLOCK_MONOTONIC)` with overflow-safe conversion | Diagnostic clock read only |
| `plat_sleep_until_ns(t)` | Absolute monotonic sleep; handle EINTR and Android behavior | Not implemented |
| `plat_thread_realtime(period)` | Conservative supported pthread policy; never assume privileged real-time access | Not implemented |
| `plat_thread_realtime_check(period)` | Return -1 where policy state cannot be reliably checked | Not implemented |
| `plat_cpu_count()` | Count CPUs available to process using supported affinity/sysconf path | Diagnostic reports 8 online CPUs; API not implemented |
| `plat_thread_guest(prio)` | Map priorities conservatively; tolerate policy denial | Not implemented |
| `plat_thread_cpu_ns()` | `CLOCK_THREAD_CPUTIME_ID` if supported | Not implemented |
| `plat_thread_perf_core(role)` | Header provides non-Windows inline no-op | No separate function needed |

## Process, system and files

| API | Android implementation plan | Initial status |
|---|---|---|
| `plat_init()` | Explicit Android init after application context is available; no desktop cwd assumptions | Not implemented |
| `plat_system_info(si)` | Android build/device/CPU/RAM fields without user-identifying paths | Partial diagnostic data only |
| `plat_exe_path(...)` | Use app/native-library path only if semantically correct; otherwise report false | Not implemented |
| `plat_os_name(...)` | Android release + kernel string, bounded output | Partial diagnostic data only |
| `plat_exec(argv)` | Do not replace Android app process; redesign caller flow | Unsupported pending call-site redesign |
| `plat_relaunch()` | Android Activity/application lifecycle request, not desktop process relaunch | Unsupported pending redesign |
| `plat_terminate()` | Avoid force-exit semantics; use Android lifecycle and return paths | No Android declaration in upstream header |
| `plat_spawn(...)` | Audit each helper; prefer in-process library or supported Android component | Unsupported pending helper inventory |
| `plat_proc_wait(...)` | Only implement if a supported child-process model is retained | Unsupported |
| `plat_proc_kill(...)` | Same as spawn/wait; no fake process handles | Unsupported |
| `plat_remove_tree(dir)` | App-private files only; do not recursively traverse external content URIs | Not implemented |
| `plat_is_link(path)` | Bionic `lstat` for app-private filesystem entries | Not implemented |
| `plat_open_file(path,flags)` | Bionic open semantics in app-private storage; SAF descriptors require a separate bridge | Not implemented |
| `plat_rename(from,to)` | Preserve no-overwrite semantics and errno behavior | Not implemented |
| `plat_onedrive_synced(dir)` | Return false | Android has no OneDrive-specific platform contract |

## Diagnostics and crash handling

| API | Android implementation plan | Initial status |
|---|---|---|
| `plat_symbol(pc,offset)` | NDK-compatible symbolization only if reliable symbol data is present | Not implemented |
| `plat_module_name(pc,...)` | Supported loader/module query; false when unknown | Not implemented |
| `plat_print_backtrace()` | NDK-compatible unwind/logcat path | Not implemented |
| `plat_fatal_notice()` | Marshal fatal notice to Java/UI safely; avoid UI calls from signal context | Not implemented |
| `plat_notice(text)` | Queue a nonfatal notice to Android UI thread | Not implemented |
| `plat_crash_handler_install()` | NDK-supported per-thread alternate stack/handler after async-signal-safety audit | Not implemented |
| `plat_window_screen_hash(...)` | Return false; desktop foreground-window capture is not an Android app contract | Unsupported by design |
| `rt_fault_watch(...)` | Keep upstream runtime fault-watch implementation; not a platform stub | Requires runtime integration |
| `rt_fault_report(...)` | Keep upstream runtime implementation; Android crash reporting integration required | Requires runtime integration |

## Device evidence received for Build #47

OnePlus CPH2943, Android 16 / SDK 36, arm64-v8a, kernel
`6.1.157-android14-11-o-gdeed0ebc8d5d`:

- Native library loaded; 8 online CPUs; 4096-byte pages.
- Exact 32-GiB guest range reservation, one-page commit/write/readback, read-only protection, PROT_NONE and release passed.
- userfaultfd open and zero-feature API query passed; available mask `0x10ffe`.
- `WP_ASYNC` and `WP_UNPOPULATED` are not advertised. The extra `PAGEFAULT_FLAG_WP` bit is also not advertised, but Build #47's corrected probe does not treat that as an additional requirement.
- soft-dirty bit 55 was NOT_OBSERVED on three pages. Do not infer universal ARM64 behavior from this.
- `PAGEMAP_SCAN` generic ioctl returned EACCES / Permission denied. This is a procfs/SELinux policy block; it does not prove ioctl absence or vendor-backport status.
- Vulkan Android surface, Mali-G615 MC2 device, logical device, queues, six-image swapchain and first blue frame passed.
- SDL event loop and simultaneous touch events passed.
- Host and SDL Activity logs show pause/stop/destroy followed by host Activity recreation/resume. No native fatal-error record is present in the supplied excerpt; this alone does not establish why the Activity was recreated.

## Implementation gates

1. Fetch and pin the exact upstream source files for each API's semantics, not only declarations.
2. Implement memory/time/system-info functions in a dedicated Android translation unit; compile against the real upstream header and Bionic NDK.
3. Keep unsupported APIs explicit; do not return fake success or dummy process handles.
4. Add app-private path/context plumbing from Java before implementing path APIs.
5. Audit every `plat_spawn`, `plat_exec`, `plat_relaunch`, and `plat_terminate` call site before defining Android behavior.
6. Verify renderer fallback when `plat_written_pages()` returns -1.
7. Only then link the backend into the upstream runtime target.

No ROM/NSP/XCI, keys, firmware, proprietary game assets, generated game code or saves belong in this repository.
