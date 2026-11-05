#include <SDL3/SDL.h>
#include "vulkan_probe.h"
#include <stdbool.h>
#include <stddef.h>

#define MAX_TRACKED_FINGERS 16

static SDL_FingerID active_fingers[MAX_TRACKED_FINGERS];
static size_t active_finger_count;

static bool track_finger_down(SDL_FingerID id)
{
    for (size_t i = 0; i < active_finger_count; ++i) {
        if (active_fingers[i] == id) {
            return true;
        }
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
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Pokémon Z-A — SDL3 Android Touch Test",
        0,
        0,
        SDL_WINDOW_FULLSCREEN
    );
    if (window == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 2;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (renderer == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 3;
    }

    za_log_vulkan_capabilities();

    bool running = true;
    SDL_Event event;
    SDL_Log("SDL3 Android touch test ready; maximum tracked simultaneous fingers=%u",
            (unsigned)MAX_TRACKED_FINGERS);

    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
                running = false;
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

        /* Neutral = no touch, green = one finger, amber = multi-touch. */
        Uint8 red = 18;
        Uint8 green = 28;
        Uint8 blue = 48;
        if (active_finger_count == 1) {
            red = 30;
            green = 120;
            blue = 70;
        } else if (active_finger_count > 1) {
            red = 170;
            green = 105;
            blue = 25;
        }

        if (!SDL_SetRenderDrawColor(renderer, red, green, blue, 255)) {
            SDL_LogError(SDL_LOG_CATEGORY_RENDER, "SDL_SetRenderDrawColor: %s", SDL_GetError());
        }
        if (!SDL_RenderClear(renderer)) {
            SDL_LogError(SDL_LOG_CATEGORY_RENDER, "SDL_RenderClear: %s", SDL_GetError());
        }
        (void)SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
