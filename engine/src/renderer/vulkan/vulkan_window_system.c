#include "defines.h"
#include "vulkan_window_system.h"

#include "utils/darray.h"

#include "vulkan_rendertarget.h"

VkResult query_support_info(vulkan_context* context, vulkan_window_system* window_system) {
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context->device.physical_device, window_system->surface, &window_system->capabilities);
    
    // Supported formats.
    // --------------------------------------
    u32 format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(context->device.physical_device, window_system->surface, &format_count, 0);

    window_system->formats = darray_reserve(VkSurfaceFormatKHR, format_count, MEMORY_TAG_RENDERER);
    vkGetPhysicalDeviceSurfaceFormatsKHR(context->device.physical_device, window_system->surface, &format_count, window_system->formats);
    darray_length_set(window_system->formats, format_count);


    // Supported present modes.
    // --------------------------------------
    u32 present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(context->device.physical_device, window_system->surface, &present_mode_count, 0);

    window_system->present_modes = darray_reserve(VkSurfaceFormatKHR, present_mode_count, MEMORY_TAG_RENDERER);
    vkGetPhysicalDeviceSurfacePresentModesKHR(context->device.physical_device, window_system->surface, &present_mode_count, window_system->present_modes);
    darray_length_set(window_system->present_modes, present_mode_count);
    return VK_SUCCESS;
}

b8 vulkan_window_system_create(
    vulkan_context* context, 
    box_platform* plat_state,
    uvec2 window_size,
    vulkan_window_system* out_window_system) {
    CHECK_VKRESULT(
        vulkan_platform_create_surface(
            context->instance, 
            plat_state,
            context->allocator, 
            &out_window_system->surface),
        "Failed to create Vulkan platform surface");
    
    CHECK_VKRESULT(
        query_support_info(context, out_window_system), 
        "Failed to query internal Vulkan surface data");
    
    out_window_system->swapchain_format = out_window_system->formats[0];
    for (u32 i = 0; i < darray_length(out_window_system->formats); ++i) {
        VkSurfaceFormatKHR format = out_window_system->formats[i];
        // Preferred formats
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            out_window_system->swapchain_format = format;
            break;
        }
    }

    VkExtent2D min = out_window_system->capabilities.minImageExtent;
    VkExtent2D max = out_window_system->capabilities.maxImageExtent;
    window_size.width = BX_CLAMP(window_size.width, min.width, max.width);
    window_size.height = BX_CLAMP(window_size.height, min.height, max.height);

    u32 image_count = out_window_system->capabilities.minImageCount + 1;
    image_count = BX_CLAMP(image_count, 0, out_window_system->capabilities.maxImageCount);

    u32 queueFamilyIndices[] = {
        context->device.mode_queues[VULKAN_QUEUE_TYPE_GRAPHICS].family_index,
        context->device.mode_queues[VULKAN_QUEUE_TYPE_PRESENT].family_index };

    // Swapchain create info 
    VkSwapchainCreateInfoKHR swapchain_create_info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    swapchain_create_info.surface = out_window_system->surface;
    swapchain_create_info.minImageCount = image_count;
    swapchain_create_info.imageFormat = out_window_system->swapchain_format.format;
    swapchain_create_info.imageColorSpace = out_window_system->swapchain_format.colorSpace;
    swapchain_create_info.preTransform = out_window_system->capabilities.currentTransform;
    swapchain_create_info.imageExtent = (VkExtent2D) { window_size.width, window_size.height };
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.clipped = TRUE;

    // Setup the queue family indices
    swapchain_create_info.pQueueFamilyIndices = queueFamilyIndices;
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_create_info.queueFamilyIndexCount = 1;

    if (context->device.mode_queues[VULKAN_QUEUE_TYPE_GRAPHICS].family_index != 
        context->device.mode_queues[VULKAN_QUEUE_TYPE_PRESENT].family_index) {
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_create_info.queueFamilyIndexCount = 2;
    }

    CHECK_VKRESULT(
        vkCreateSwapchainKHR(
            context->device.logical_device,
            &swapchain_create_info,
            context->allocator,
            &out_window_system->swapchain),
        "Failed to create internal Vulkan swapchain");

    CHECK_VKRESULT(
        vkGetSwapchainImagesKHR(
            context->device.logical_device, 
            out_window_system->swapchain, 
            &out_window_system->swapchain_image_count,
            0),
        "Failed to obtain internal Vulkan swapchain images");

    VkImage* images = (VkImage*)ballocate(sizeof(VkImage) * out_window_system->swapchain_image_count, MEMORY_TAG_RENDERER);
    vkGetSwapchainImagesKHR(context->device.logical_device, out_window_system->swapchain, &out_window_system->swapchain_image_count, images);

    out_window_system->swapchain_images = (vulkan_image*)ballocate(sizeof(vulkan_image) * out_window_system->swapchain_image_count, MEMORY_TAG_RENDERER);
    for (u32 i = 0; i < out_window_system->swapchain_image_count; ++i) {
        out_window_system->swapchain_images[i].handle = images[i];

        VkImageViewCreateInfo view_info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        view_info.image = out_window_system->swapchain_images[i].handle;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = out_window_system->swapchain_format.format;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        CHECK_VKRESULT(
            vkCreateImageView(
                context->device.logical_device, 
                &view_info, 
                context->allocator, 
                &out_window_system->swapchain_images[i].view),
            "Failed to create internal Vulkan image view");
    }
    return TRUE;
}

void vulkan_window_system_destroy(vulkan_context* context, vulkan_window_system* window_system) {
    bfree(window_system->swapchain_images, sizeof(VkImage) * window_system->swapchain_image_count, MEMORY_TAG_RENDERER);
    window_system->swapchain_images = 0;

    vkDestroySwapchainKHR(context->device.logical_device, window_system->swapchain, context->allocator);

    darray_destroy(window_system->formats);
    darray_destroy(window_system->present_modes);

    vkDestroySurfaceKHR(context->instance, window_system->surface, context->allocator);
}

b8 vulkan_window_system_connect_rendertarget(vulkan_context* context, vulkan_window_system* window_system, box_rendertarget** out_rendertarget) {
    
    if (!vulkan_rendertarget_create_external(
        context, 
        window_system->swapchain_images, 
        window_system->swapchain_image_count, 
        &window_system->surface_rendertarget)) {
        return FALSE;
    }

    *out_rendertarget = &window_system->surface_rendertarget;
    return TRUE;
}

b8 vulkan_window_system_acquire_frame(vulkan_context *context, vulkan_window_system *window_system, f64 delta_time, u64 timeout) {
    return TRUE;
}

b8 vulkan_window_system_present(vulkan_context *context, vulkan_window_system *window_system, VkQueue queue, VkSemaphore *wait_semaphores, u32 wait_semaphore_count) {
    return TRUE;
}
