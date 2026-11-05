#ifndef ZA_ANDROID_VULKAN_PROBE_H
#define ZA_ANDROID_VULKAN_PROBE_H
#include <SDL3/SDL.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Tests Android Vulkan loader, SDL surface, and device presentation support. */
void za_log_vulkan_capabilities(SDL_Window *window);
#ifdef __cplusplus
}
#endif
#endif
