#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

#include <stdint.h>
#include <stdlib.h>

void za_log_vulkan_capabilities(void)
{
    VkApplicationInfo app = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Pokemon ZA Android Probe",
        .applicationVersion = VK_MAKE_API_VERSION(0, 0, 3, 0),
        .pEngineName = "SDL3 diagnostic shell",
        .engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
        .apiVersion = VK_API_VERSION_1_0
    };
    VkInstanceCreateInfo create = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app
    };
    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&create, NULL, &instance);
    if (result != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "Vulkan instance creation failed: VkResult=%d", (int)result);
        return;
    }

    uint32_t count = 0;
    result = vkEnumeratePhysicalDevices(instance, &count, NULL);
    if (result != VK_SUCCESS || count == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "No Vulkan physical devices: result=%d count=%u",
                     (int)result, (unsigned)count);
        vkDestroyInstance(instance, NULL);
        return;
    }

    VkPhysicalDevice *devices = calloc(count, sizeof(*devices));
    if (devices == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Vulkan device list allocation failed");
        vkDestroyInstance(instance, NULL);
        return;
    }

    result = vkEnumeratePhysicalDevices(instance, &count, devices);
    if (result != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "Vulkan device enumeration failed: VkResult=%d", (int)result);
        free(devices);
        vkDestroyInstance(instance, NULL);
        return;
    }

    SDL_Log("VULKAN_INSTANCE_OK physical_device_count=%u", (unsigned)count);
    for (uint32_t i = 0; i < count; ++i) {
        VkPhysicalDeviceProperties props;
        VkPhysicalDeviceMemoryProperties memory;
        vkGetPhysicalDeviceProperties(devices[i], &props);
        vkGetPhysicalDeviceMemoryProperties(devices[i], &memory);

        uint64_t device_local_bytes = 0;
        for (uint32_t h = 0; h < memory.memoryHeapCount; ++h) {
            if ((memory.memoryHeaps[h].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0) {
                device_local_bytes += memory.memoryHeaps[h].size;
            }
        }

        uint32_t extension_count = 0;
        VkResult ext_result =
            vkEnumerateDeviceExtensionProperties(devices[i], NULL, &extension_count, NULL);

        SDL_Log("VULKAN_DEVICE index=%u name=%s vendor=0x%04x device=0x%04x type=%u "
                "api=%u.%u.%u driver=%u local_heap_mib=%llu extensions=%u ext_result=%d",
                (unsigned)i, props.deviceName, (unsigned)props.vendorID,
                (unsigned)props.deviceID, (unsigned)props.deviceType,
                (unsigned)VK_API_VERSION_MAJOR(props.apiVersion),
                (unsigned)VK_API_VERSION_MINOR(props.apiVersion),
                (unsigned)VK_API_VERSION_PATCH(props.apiVersion),
                (unsigned)props.driverVersion,
                (unsigned long long)(device_local_bytes / (1024u * 1024u)),
                (unsigned)extension_count, (int)ext_result);
    }

    free(devices);
    vkDestroyInstance(instance, NULL);
}
