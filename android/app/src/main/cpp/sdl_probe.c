#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h> /* Exports the SDL_main entry point required by SDLActivity on Android. */
#include <SDL3/SDL_system.h>
#include "vulkan_probe.h"
#include "guest_memory_probe.h"
#include "guest_write_tracking_probe.h"
#include "android_platform_probe.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdatomic.h>

#define MAX_TRACKED_FINGERS 16
#define DIAGNOSTIC_LOG_NAME "za_sdl_diagnostics.log"

static SDL_FingerID active_fingers[MAX_TRACKED_FINGERS];
static size_t active_finger_count;

static atomic_bool app_in_background = ATOMIC_VAR_INIT(false);
static atomic_uint lifecycle_event_count = ATOMIC_VAR_INIT(0);

/* SDL documents Android lifecycle events as event-watch notifications. Keep
   this callback tiny: no renderer/window work is allowed on the callback path. */
static bool SDLCALL lifecycle_event_watch(void *userdata, SDL_Event *event)
{
    (void)userdata;
    if (!event) return true;

    switch (event->type) {
    case SDL_EVENT_WILL_ENTER_BACKGROUND:
        atomic_store_explicit(&app_in_background, true, memory_order_release);
        atomic_fetch_add_explicit(&lifecycle_event_count, 1, memory_order_relaxed);
        SDL_Log("ZA_ANDROID_LIFECYCLE event=WILL_ENTER_BACKGROUND render=paused");
        break;
    case SDL_EVENT_DID_ENTER_BACKGROUND:
        atomic_store_explicit(&app_in_background, true, memory_order_release);
        atomic_fetch_add_explicit(&lifecycle_event_count, 1, memory_order_relaxed);
        SDL_Log("ZA_ANDROID_LIFECYCLE event=DID_ENTER_BACKGROUND render=paused");
        break;
    case SDL_EVENT_WILL_ENTER_FOREGROUND:
        atomic_fetch_add_explicit(&lifecycle_event_count, 1, memory_order_relaxed);
        SDL_Log("ZA_ANDROID_LIFECYCLE event=WILL_ENTER_FOREGROUND");
        break;
    case SDL_EVENT_DID_ENTER_FOREGROUND:
        atomic_store_explicit(&app_in_background, false, memory_order_release);
        atomic_fetch_add_explicit(&lifecycle_event_count, 1, memory_order_relaxed);
        SDL_Log("ZA_ANDROID_LIFECYCLE event=DID_ENTER_FOREGROUND render=resume_requested");
        break;
    case SDL_EVENT_LOW_MEMORY:
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "ZA_ANDROID_LIFECYCLE event=LOW_MEMORY");
        break;
    default:
        break;
    }
    return true;
}

/*
 * SDL_Log normally goes to Android logcat. The host screen cannot reliably
 * read logcat on modern Android, so mirror every SDL/Vulkan log line into
 * the same app-private file that SDLProbeActivity and MainActivity use.
 */
static void persist_sdl_log(void *userdata, int category,
                            SDL_LogPriority priority, const char *message)
{
    (void)userdata;
    if (message == NULL) return;

    const char *directory = SDL_GetAndroidInternalStoragePath();
    if (directory == NULL) return;

    char path[1024];
    int written = snprintf(path, sizeof(path), "%s/%s",
                           directory, DIAGNOSTIC_LOG_NAME);
    if (written <= 0 || (size_t)written >= sizeof(path)) return;

    FILE *file = fopen(path, "a");
    if (file == NULL) return;
    (void)fprintf(file, "SDL_NATIVE category=%d priority=%d %s\n",
                  category, (int)priority, message);
    (void)fflush(file);
    (void)fclose(file);
}

static bool track_finger_down(SDL_FingerID id)
{
    for (size_t i = 0; i < active_finger_count; ++i) {
        if (active_fingers[i] == id) return true;
    }
    if (active_finger_count == MAX_TRACKED_FINGERS) {
        SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "Touch tracker full; ignoring finger=%llu",
                    (unsigned long long)id);
        return false;
    }
    active_fingers[active_finger_count++] = id;
    return true;
}

static void track_finger_up(SDL_FingerID id)
{
    for (size_t i = 0; i < active_finger_count; ++i) {
        if (active_fingers[i] == id) {
            active_fingers[i] = active_fingers[active_finger_count - 1];
            --active_finger_count;
            return;
        }
    }
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "ZA_DIAG_STAGE=SDL_INIT_FAILED: %s",
                     SDL_GetError());
        return 1;
    }

    SDL_SetLogOutputFunction(persist_sdl_log, NULL);
    SDL_AddEventWatch(lifecycle_event_watch, NULL);
    SDL_Log("ZA_ANDROID_LIFECYCLE result=WATCH_REGISTERED");
    const char *log_directory = SDL_GetAndroidInternalStoragePath();
    SDL_Log("ZA_DIAG_STAGE=native_log_capture_ready path=%s",
            log_directory != NULL ? log_directory : "(unavailable)");
    SDL_Log("ZA_DIAG_STAGE=starting SDL3");
    (void)za_run_android_platform_api_probe();
    za_run_guest_memory_probe();
    za_run_guest_write_tracking_probe();

    const SDL_DisplayID display = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode *mode = display ? SDL_GetCurrentDisplayMode(display) : NULL;
    int width = (mode && mode->w > 0) ? mode->w : 1280;
    int height = (mode && mode->h > 0) ? mode->h : 720;
    SDL_Log("ZA_DIAG_STAGE=creating_window display=%u size=%dx%d",
            (unsigned)display, width, height);

    SDL_Window *window = SDL_CreateWindow(
        "Pokemon Z-A Android Diagnostic",
        width,
        height,
        SDL_WINDOW_FULLSCREEN | SDL_WINDOW_VULKAN
    );
    if (window == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "ZA_DIAG_STAGE=WINDOW_FAILED: %s",
                     SDL_GetError());
        SDL_Quit();
        return 2;
    }

    SDL_Log("ZA_DIAG_STAGE=window_created");
    za_log_vulkan_capabilities(window);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (renderer == NULL) {
        SDL_LogWarn(SDL_LOG_CATEGORY_RENDER, "Default renderer unavailable: %s",
                    SDL_GetError());
        renderer = SDL_CreateRenderer(window, "software");
    }
    if (renderer == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                     "ZA_DIAG_STAGE=RENDERER_UNAVAILABLE; keeping diagnostic window open: %s",
                     SDL_GetError());
    } else {
        SDL_Log("ZA_DIAG_STAGE=renderer_created");
    }

    SDL_Log("ZA_DIAG_STAGE=event_loop_ready");
    bool running = true;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_WINDOW_SHOWN:
                SDL_Log("ZA_ANDROID_LIFECYCLE event=WINDOW_SHOWN");
                break;
            case SDL_EVENT_WINDOW_HIDDEN:
                SDL_Log("ZA_ANDROID_LIFECYCLE event=WINDOW_HIDDEN");
                break;
            case SDL_EVENT_WINDOW_MINIMIZED:
                SDL_Log("ZA_ANDROID_LIFECYCLE event=WINDOW_MINIMIZED");
                break;
            case SDL_EVENT_WINDOW_RESTORED:
                SDL_Log("ZA_ANDROID_LIFECYCLE event=WINDOW_RESTORED");
                break;
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                SDL_Log("ZA_ANDROID_LIFECYCLE event=WINDOW_FOCUS_LOST");
                break;
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                SDL_Log("ZA_ANDROID_LIFECYCLE event=WINDOW_FOCUS_GAINED");
                break;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                SDL_Log("ZA_ANDROID_LIFECYCLE event=WINDOW_PIXEL_SIZE_CHANGED width=%d height=%d",
                        event.window.data1, event.window.data2);
                break;
            case SDL_EVENT_RENDER_DEVICE_RESET:
                SDL_LogWarn(SDL_LOG_CATEGORY_RENDER,
                            "ZA_ANDROID_LIFECYCLE event=RENDER_DEVICE_RESET SDL renderer owns device recovery");
                break;
            case SDL_EVENT_RENDER_DEVICE_LOST:
                SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                             "ZA_ANDROID_LIFECYCLE event=RENDER_DEVICE_LOST");
                break;
            case SDL_EVENT_FINGER_DOWN:
                (void)track_finger_down(event.tfinger.fingerID);
                SDL_Log("TOUCH_DOWN id=%llu x=%.4f y=%.4f active=%u",
                        (unsigned long long)event.tfinger.fingerID,
                        event.tfinger.x, event.tfinger.y,
                        (unsigned)active_finger_count);
                break;
            case SDL_EVENT_FINGER_UP:
                track_finger_up(event.tfinger.fingerID);
                SDL_Log("TOUCH_UP id=%llu active=%u",
                        (unsigned long long)event.tfinger.fingerID,
                        (unsigned)active_finger_count);
                break;
            case SDL_EVENT_FINGER_MOTION:
                SDL_LogVerbose(SDL_LOG_CATEGORY_INPUT,
                               "TOUCH_MOVE id=%llu x=%.4f y=%.4f dx=%.4f dy=%.4f",
                               (unsigned long long)event.tfinger.fingerID,
                               event.tfinger.x, event.tfinger.y,
                               event.tfinger.dx, event.tfinger.dy);
                break;
            default:
                break;
            }
        }

        if (renderer != NULL && !atomic_load_explicit(&app_in_background, memory_order_acquire)) {
            Uint8 red = 18, green = 28, blue = 48;
            if (active_finger_count == 1) {
                red = 30; green = 120; blue = 70;
            } else if (active_finger_count > 1) {
                red = 170; green = 105; blue = 25;
            }
            (void)SDL_SetRenderDrawColor(renderer, red, green, blue, 255);
            (void)SDL_RenderClear(renderer);
            (void)SDL_RenderPresent(renderer);
        }
        SDL_Delay(16);
    }

    SDL_RemoveEventWatch(lifecycle_event_watch, NULL);
    SDL_Log("ZA_ANDROID_LIFECYCLE result=SUMMARY events=%u background=%s",
            atomic_load_explicit(&lifecycle_event_count, memory_order_relaxed),
            atomic_load_explicit(&app_in_background, memory_order_acquire) ? "yes" : "no");
    if (renderer != NULL) SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
