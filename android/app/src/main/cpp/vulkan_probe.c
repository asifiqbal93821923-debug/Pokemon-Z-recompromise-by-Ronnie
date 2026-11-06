#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

static void log_vk_error(const char *stage, VkResult result)
{
    SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "ZA_VK_STAGE=%s VkResult=%d", stage, (int)result);
}

void za_log_vulkan_capabilities(SDL_Window *window)
{
    if (window == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "ZA_VK_STAGE=missing_window");
        return;
    }
    SDL_Log("ZA_VK_STAGE=loading_android_vulkan_loader");
    if (!SDL_Vulkan_LoadLibrary(NULL)) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "ZA_VK_STAGE=loader_failed: %s", SDL_GetError());
        return;
    }
    uint32_t ext_count = 0;
    const char * const *exts = SDL_Vulkan_GetInstanceExtensions(&ext_count);
    if (exts == NULL || ext_count == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "ZA_VK_STAGE=instance_extensions_failed: %s", SDL_GetError());
        SDL_Vulkan_UnloadLibrary();
        return;
    }
    VkApplicationInfo app = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Pokemon ZA Android Surface Probe",
        .applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
        .pEngineName = "SDL3 Android diagnostic",
        .engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
        .apiVersion = VK_API_VERSION_1_0
    };
    VkInstanceCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app,
        .enabledExtensionCount = ext_count,
        .ppEnabledExtensionNames = exts
    };
    VkInstance instance = VK_NULL_HANDLE;
    VkResult vr = vkCreateInstance(&ci, NULL, &instance);
    if (vr != VK_SUCCESS) {
        log_vk_error("instance_create_failed", vr);
        SDL_Vulkan_UnloadLibrary();
        return;
    }
    SDL_Log("ZA_VK_STAGE=instance_created extensions=%u", (unsigned)ext_count);

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, instance, NULL, &surface)) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "ZA_VK_STAGE=surface_create_failed: %s", SDL_GetError());
        vkDestroyInstance(instance, NULL);
        SDL_Vulkan_UnloadLibrary();
        return;
    }
    SDL_Log("ZA_VK_STAGE=android_surface_created");

    uint32_t count = 0;
    vr = vkEnumeratePhysicalDevices(instance, &count, NULL);
    if (vr != VK_SUCCESS || count == 0) {
        log_vk_error("physical_device_enumeration_failed", vr);
        vkDestroySurfaceKHR(instance, surface, NULL);
        vkDestroyInstance(instance, NULL);
        SDL_Vulkan_UnloadLibrary();
        return;
    }
    VkPhysicalDevice *devices = calloc(count, sizeof(*devices));
    if (devices == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "ZA_VK_STAGE=device_allocation_failed");
        vkDestroySurfaceKHR(instance, surface, NULL);
        vkDestroyInstance(instance, NULL);
        SDL_Vulkan_UnloadLibrary();
        return;
    }
    vr = vkEnumeratePhysicalDevices(instance, &count, devices);
    if (vr != VK_SUCCESS) {
        log_vk_error("physical_device_list_failed", vr);
        free(devices);
        vkDestroySurfaceKHR(instance, surface, NULL);
        vkDestroyInstance(instance, NULL);
        SDL_Vulkan_UnloadLibrary();
        return;
    }
    SDL_Log("ZA_VK_STAGE=physical_devices_found count=%u", (unsigned)count);
    for (uint32_t d = 0; d < count; ++d) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devices[d], &props);
        uint32_t qcount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[d], &qcount, NULL);
        VkQueueFamilyProperties *queues = calloc(qcount, sizeof(*queues));
        if (queues == NULL) {
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "ZA_VK_STAGE=queue_allocation_failed");
            continue;
        }
        vkGetPhysicalDeviceQueueFamilyProperties(devices[d], &qcount, queues);
        bool can_present = false;
        for (uint32_t q = 0; q < qcount; ++q) {
            VkBool32 supported = VK_FALSE;
            VkResult sr = vkGetPhysicalDeviceSurfaceSupportKHR(devices[d], q, surface, &supported);
            if (sr == VK_SUCCESS && supported == VK_TRUE) can_present = true;
        }
        SDL_Log("ZA_VK_DEVICE name=%s vendor=0x%04x device=0x%04x api=%u.%u.%u "
                "queue_families=%u android_surface_present=%s",
                props.deviceName, (unsigned)props.vendorID, (unsigned)props.deviceID,
                (unsigned)VK_API_VERSION_MAJOR(props.apiVersion),
                (unsigned)VK_API_VERSION_MINOR(props.apiVersion),
                (unsigned)VK_API_VERSION_PATCH(props.apiVersion),
                (unsigned)qcount, can_present ? "yes" : "no");
        free(queues);
        VkSurfaceCapabilitiesKHR caps;
        vr = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(devices[d], surface, &caps);
        if (vr == VK_SUCCESS) {
            SDL_Log("ZA_VK_SURFACE_CAPS min_images=%u max_images=%u extent=%ux%u usage=0x%x",
                    (unsigned)caps.minImageCount, (unsigned)caps.maxImageCount,
                    (unsigned)caps.currentExtent.width, (unsigned)caps.currentExtent.height,
                    (unsigned)caps.supportedUsageFlags);
        } else {
            log_vk_error("surface_capabilities_failed", vr);
        }
    }
    free(devices);
    vkDestroySurfaceKHR(instance, surface, NULL);
    vkDestroyInstance(instance, NULL);
    SDL_Vulkan_UnloadLibrary();
    SDL_Log("ZA_VK_STAGE=surface_probe_complete");
}
