#include <SDL3/SDL.h>
#include <stdbool.h>

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Pokémon Z-A — SDL3 Android Probe",
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
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 3;
    }

    bool running = true;
    bool touch_active = false;
    SDL_Event event;

    SDL_Log("SDL3 Android probe started. Touch events are being monitored.");

    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_FINGER_DOWN:
                touch_active = true;
                SDL_Log("Touch down: finger=%lld x=%.3f y=%.3f",
                        (long long)event.tfinger.fingerID,
                        event.tfinger.x, event.tfinger.y);
                break;
            case SDL_EVENT_FINGER_UP:
                touch_active = false;
                SDL_Log("Touch up: finger=%lld",
                        (long long)event.tfinger.fingerID);
                break;
            case SDL_EVENT_FINGER_MOTION:
                SDL_LogVerbose(SDL_LOG_CATEGORY_INPUT,
                               "Touch move: finger=%lld x=%.3f y=%.3f",
                               (long long)event.tfinger.fingerID,
                               event.tfinger.x, event.tfinger.y);
                break;
            default:
                break;
            }
        }

        if (!SDL_SetRenderDrawColor(renderer,
                                    touch_active ? 40 : 18,
                                    touch_active ? 110 : 28,
                                    touch_active ? 180 : 48,
                                    255)) {
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
