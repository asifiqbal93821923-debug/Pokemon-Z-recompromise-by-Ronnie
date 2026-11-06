#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void log_vk_error(const char *stage, VkResult result)
{
    SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "ZA_VK_STAGE=%s VkResult=%d", stage, (int)result);
}

static bool has_device_extension(VkPhysicalDevice device, const char *name)
{
    uint32_t count = 0;
    if (vkEnumerateDeviceExtensionProperties(device, NULL, &count, NULL) != VK_SUCCESS) return false;
    VkExtensionProperties *items = calloc(count, sizeof(*items));
    if (items == NULL) return false;
    VkResult result = vkEnumerateDeviceExtensionProperties(device, NULL, &count, items);
    bool found = false;
    if (result == VK_SUCCESS) {
        for (uint32_t i = 0; i < count; ++i) {
            if (strcmp(items[i].extensionName, name) == 0) { found = true; break; }
        }
    }
    free(items);
    return found;
}

static bool choose_device(VkPhysicalDevice device, VkSurfaceKHR surface,
                          uint32_t *graphics_family, uint32_t *present_family)
{
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, NULL);
    VkQueueFamilyProperties *families = calloc(count, sizeof(*families));
    if (families == NULL) return false;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families);
    bool have_graphics = false, have_present = false;
    for (uint32_t i = 0; i < count; ++i) {
        if (!have_graphics && (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            *graphics_family = i; have_graphics = true;
        }
        VkBool32 supported = VK_FALSE;
        if (vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &supported) == VK_SUCCESS &&
            supported == VK_TRUE) {
            *present_family = i; have_present = true;
        }
        if (have_graphics && have_present) break;
    }
    free(families);
    return have_graphics && have_present &&
           has_device_extension(device, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
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
        .pApplicationName = "Pokemon ZA Android Vulkan Frame Probe",
        .applicationVersion = VK_MAKE_API_VERSION(0, 0, 2, 0),
        .pEngineName = "SDL3 Android diagnostic",
        .engineVersion = VK_MAKE_API_VERSION(0, 0, 2, 0),
        .apiVersion = VK_API_VERSION_1_0
    };
    VkInstanceCreateInfo ici = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app,
        .enabledExtensionCount = ext_count,
        .ppEnabledExtensionNames = exts
    };
    VkInstance instance = VK_NULL_HANDLE;
    VkResult vr = vkCreateInstance(&ici, NULL, &instance);
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

    uint32_t device_count = 0;
    vr = vkEnumeratePhysicalDevices(instance, &device_count, NULL);
    if (vr != VK_SUCCESS || device_count == 0) {
        log_vk_error("physical_device_enumeration_failed", vr);
        goto cleanup_surface;
    }
    VkPhysicalDevice *devices = calloc(device_count, sizeof(*devices));
    if (devices == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "ZA_VK_STAGE=device_allocation_failed");
        goto cleanup_surface;
    }
    vr = vkEnumeratePhysicalDevices(instance, &device_count, devices);
    if (vr != VK_SUCCESS) {
        log_vk_error("physical_device_list_failed", vr);
        free(devices);
        goto cleanup_surface;
    }

    VkPhysicalDevice physical = VK_NULL_HANDLE;
    uint32_t graphics_family = 0, present_family = 0;
    for (uint32_t i = 0; i < device_count; ++i) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devices[i], &props);
        uint32_t gf = 0, pf = 0;
        bool usable = choose_device(devices[i], surface, &gf, &pf);
        SDL_Log("ZA_VK_DEVICE name=%s vendor=0x%04x device=0x%04x api=%u.%u.%u "
                "swapchain_extension=%s graphics_present_queues=%s",
                props.deviceName, (unsigned)props.vendorID, (unsigned)props.deviceID,
                (unsigned)VK_API_VERSION_MAJOR(props.apiVersion),
                (unsigned)VK_API_VERSION_MINOR(props.apiVersion),
                (unsigned)VK_API_VERSION_PATCH(props.apiVersion),
                has_device_extension(devices[i], VK_KHR_SWAPCHAIN_EXTENSION_NAME) ? "yes" : "no",
                usable ? "yes" : "no");
        if (usable && physical == VK_NULL_HANDLE) {
            physical = devices[i]; graphics_family = gf; present_family = pf;
        }
    }
    free(devices);
    if (physical == VK_NULL_HANDLE) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "ZA_VK_STAGE=no_usable_device (graphics+present+VK_KHR_swapchain required)");
        goto cleanup_surface;
    }

    float priority = 1.0f;
    VkDeviceQueueCreateInfo queues[2] = {
        { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .queueFamilyIndex = graphics_family, .queueCount = 1, .pQueuePriorities = &priority },
        { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .queueFamilyIndex = present_family, .queueCount = 1, .pQueuePriorities = &priority }
    };
    VkDeviceCreateInfo dci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = (graphics_family == present_family) ? 1u : 2u,
        .pQueueCreateInfos = queues,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = (const char *[]){ VK_KHR_SWAPCHAIN_EXTENSION_NAME }
    };
    VkDevice device = VK_NULL_HANDLE;
    vr = vkCreateDevice(physical, &dci, NULL, &device);
    if (vr != VK_SUCCESS) {
        log_vk_error("logical_device_create_failed", vr);
        goto cleanup_surface;
    }
    SDL_Log("ZA_VK_STAGE=logical_device_created graphics_queue=%u present_queue=%u",
            graphics_family, present_family);

    VkQueue graphics_queue = VK_NULL_HANDLE, present_queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, graphics_family, 0, &graphics_queue);
    vkGetDeviceQueue(device, present_family, 0, &present_queue);

    VkSurfaceCapabilitiesKHR caps;
    vr = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &caps);
    if (vr != VK_SUCCESS) {
        log_vk_error("surface_capabilities_failed", vr);
        goto cleanup_device;
    }
    if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "ZA_VK_STAGE=swapchain_transfer_dst_unsupported");
        goto cleanup_device;
    }
    uint32_t format_count = 0;
    vr = vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &format_count, NULL);
    if (vr != VK_SUCCESS || format_count == 0) {
        log_vk_error("surface_formats_failed", vr);
        goto cleanup_device;
    }
    VkSurfaceFormatKHR *formats = calloc(format_count, sizeof(*formats));
    if (formats == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "ZA_VK_STAGE=format_allocation_failed");
        goto cleanup_device;
    }
    vr = vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &format_count, formats);
    if (vr != VK_SUCCESS) {
        log_vk_error("surface_format_list_failed", vr);
        free(formats);
        goto cleanup_device;
    }
    VkSurfaceFormatKHR chosen = formats[0];
    for (uint32_t i = 0; i < format_count; ++i) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM ||
            formats[i].format == VK_FORMAT_R8G8B8A8_UNORM) { chosen = formats[i]; break; }
    }
    free(formats);

    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        extent.width = (uint32_t)((w > 0) ? w : 1280);
        extent.height = (uint32_t)((h > 0) ? h : 720);
        if (extent.width < caps.minImageExtent.width) extent.width = caps.minImageExtent.width;
        if (extent.width > caps.maxImageExtent.width) extent.width = caps.maxImageExtent.width;
        if (extent.height < caps.minImageExtent.height) extent.height = caps.minImageExtent.height;
        if (extent.height > caps.maxImageExtent.height) extent.height = caps.maxImageExtent.height;
    }
    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount && image_count > caps.maxImageCount) image_count = caps.maxImageCount;
    VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (!(caps.supportedCompositeAlpha & alpha)) {
        const VkCompositeAlphaFlagBitsKHR choices[] = {
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
        };
        for (uint32_t i = 0; i < 3; ++i) {
            if (caps.supportedCompositeAlpha & choices[i]) { alpha = choices[i]; break; }
        }
    }
    uint32_t family_indices[] = { graphics_family, present_family };
    VkSwapchainCreateInfoKHR sci = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = image_count,
        .imageFormat = chosen.format,
        .imageColorSpace = chosen.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = (graphics_family == present_family) ?
                            VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
        .queueFamilyIndexCount = (graphics_family == present_family) ? 0u : 2u,
        .pQueueFamilyIndices = (graphics_family == present_family) ? NULL : family_indices,
        .preTransform = caps.currentTransform,
        .compositeAlpha = alpha,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE
    };
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    vr = vkCreateSwapchainKHR(device, &sci, NULL, &swapchain);
    if (vr != VK_SUCCESS) {
        log_vk_error("swapchain_create_failed", vr);
        goto cleanup_device;
    }
    SDL_Log("ZA_VK_STAGE=swapchain_created extent=%ux%u format=%d images=%u",
            extent.width, extent.height, (int)chosen.format, image_count);

    uint32_t swap_image_count = 0;
    vr = vkGetSwapchainImagesKHR(device, swapchain, &swap_image_count, NULL);
    if (vr != VK_SUCCESS || swap_image_count == 0) {
        log_vk_error("swapchain_images_failed", vr);
        goto cleanup_swapchain;
    }
    VkImage *swap_images = calloc(swap_image_count, sizeof(*swap_images));
    if (swap_images == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "ZA_VK_STAGE=swapchain_image_allocation_failed");
        goto cleanup_swapchain;
    }
    vr = vkGetSwapchainImagesKHR(device, swapchain, &swap_image_count, swap_images);
    if (vr != VK_SUCCESS) {
        log_vk_error("swapchain_image_list_failed", vr);
        free(swap_images);
        goto cleanup_swapchain;
    }

    VkCommandPoolCreateInfo pci = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphics_family
    };
    VkCommandPool pool = VK_NULL_HANDLE;
    vr = vkCreateCommandPool(device, &pci, NULL, &pool);
    if (vr != VK_SUCCESS) {
        log_vk_error("command_pool_create_failed", vr);
        free(swap_images);
        goto cleanup_swapchain;
    }
    VkCommandBufferAllocateInfo cai = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer command = VK_NULL_HANDLE;
    vr = vkAllocateCommandBuffers(device, &cai, &command);
    if (vr != VK_SUCCESS) {
        log_vk_error("command_buffer_allocate_failed", vr);
        vkDestroyCommandPool(device, pool, NULL);
        free(swap_images);
        goto cleanup_swapchain;
    }
    VkSemaphoreCreateInfo sem_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkSemaphore acquired = VK_NULL_HANDLE, rendered = VK_NULL_HANDLE;
    vr = vkCreateSemaphore(device, &sem_info, NULL, &acquired);
    if (vr == VK_SUCCESS) vr = vkCreateSemaphore(device, &sem_info, NULL, &rendered);
    if (vr != VK_SUCCESS) {
        log_vk_error("semaphore_create_failed", vr);
        if (acquired) vkDestroySemaphore(device, acquired, NULL);
        vkFreeCommandBuffers(device, pool, 1, &command);
        vkDestroyCommandPool(device, pool, NULL);
        free(swap_images);
        goto cleanup_swapchain;
    }
    uint32_t image_index = 0;
    vr = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, acquired, VK_NULL_HANDLE, &image_index);
    if (vr != VK_SUCCESS && vr != VK_SUBOPTIMAL_KHR) {
        log_vk_error("acquire_image_failed", vr);
        goto cleanup_sync;
    }
    VkCommandBufferBeginInfo begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
    vr = vkBeginCommandBuffer(command, &begin);
    if (vr == VK_SUCCESS) {
        VkImageMemoryBarrier to_transfer = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = swap_images[image_index],
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &to_transfer);
        VkClearColorValue color = { .float32 = { 0.08f, 0.28f, 0.52f, 1.0f } };
        VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCmdClearColorImage(command, swap_images[image_index],
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &range);
        VkImageMemoryBarrier to_present = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = 0,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = swap_images[image_index],
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &to_present);
        vr = vkEndCommandBuffer(command);
    }
    if (vr != VK_SUCCESS) {
        log_vk_error("record_clear_frame_failed", vr);
        goto cleanup_sync;
    }
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1, .pWaitSemaphores = &acquired,
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1, .pCommandBuffers = &command,
        .signalSemaphoreCount = 1, .pSignalSemaphores = &rendered
    };
    vr = vkQueueSubmit(graphics_queue, 1, &submit, VK_NULL_HANDLE);
    if (vr != VK_SUCCESS) {
        log_vk_error("queue_submit_failed", vr);
        goto cleanup_sync;
    }
    VkPresentInfoKHR present = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1, .pWaitSemaphores = &rendered,
        .swapchainCount = 1, .pSwapchains = &swapchain,
        .pImageIndices = &image_index
    };
    vr = vkQueuePresentKHR(present_queue, &present);
    if (vr != VK_SUCCESS && vr != VK_SUBOPTIMAL_KHR) {
        log_vk_error("queue_present_failed", vr);
        vkDeviceWaitIdle(device);
        goto cleanup_sync;
    }
    vkQueueWaitIdle(present_queue);
    SDL_Log("ZA_VK_STAGE=first_frame_presented color=blue image_index=%u", image_index);

cleanup_sync:
    vkDeviceWaitIdle(device);
    vkDestroySemaphore(device, rendered, NULL);
    vkDestroySemaphore(device, acquired, NULL);
    vkFreeCommandBuffers(device, pool, 1, &command);
    vkDestroyCommandPool(device, pool, NULL);
    free(swap_images);
cleanup_swapchain:
    vkDestroySwapchainKHR(device, swapchain, NULL);
cleanup_device:
    vkDeviceWaitIdle(device);
    vkDestroyDevice(device, NULL);
cleanup_surface:
    vkDestroySurfaceKHR(instance, surface, NULL);
    vkDestroyInstance(instance, NULL);
    SDL_Vulkan_UnloadLibrary();
    SDL_Log("ZA_VK_STAGE=frame_probe_complete");
}
