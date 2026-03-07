#include "defines.h"
#include "vulkan_swapchain.h"

#include "utils/darray.h"

#include "vulkan_device.h"

VkSurfaceFormatKHR find_swapchain_format(vulkan_swapchain_support_info* swapchain_info) {
    for (u32 i = 0; i < darray_length(swapchain_info->formats); ++i) {
        VkSurfaceFormatKHR format = swapchain_info->formats[i];
        // Preferred formats
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    return swapchain_info->formats[0];
}


VkResult vulkan_swapchain_create(
    vulkan_context* context,
    vec2 size,
    vulkan_swapchain* out_swapchain) {
    vulkan_swapchain_support_info* swapchain_info = &context->device.swapchain_support;
    out_swapchain->image_format = find_swapchain_format(swapchain_info);
    
    VkExtent2D min = swapchain_info->capabilities.minImageExtent;
    VkExtent2D max = swapchain_info->capabilities.maxImageExtent;
    size.width = BX_CLAMP(size.width, min.width, max.width);
    size.height = BX_CLAMP(size.height, min.height, max.height);

    u32 image_count = context->config.frames_in_flight;
    image_count = BX_CLAMP(image_count, 0, swapchain_info->capabilities.maxImageCount);
    
    u32 queueFamilyIndices[] = {
        context->device.mode_queues[VULKAN_QUEUE_TYPE_GRAPHICS].family_index,
        context->device.mode_queues[VULKAN_QUEUE_TYPE_PRESENT].family_index };

    // Swapchain create info
    VkSwapchainCreateInfoKHR swapchain_create_info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    swapchain_create_info.surface = context->surface;
    swapchain_create_info.minImageCount = image_count;
    swapchain_create_info.imageFormat = out_swapchain->image_format.format;
    swapchain_create_info.imageColorSpace = out_swapchain->image_format.colorSpace;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_create_info.imageExtent = (VkExtent2D) { size.width, size.height };
    swapchain_create_info.preTransform = context->device.swapchain_support.capabilities.currentTransform;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchain_create_info.oldSwapchain = out_swapchain->handle;
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.clipped = TRUE;

    // Setup the queue family indices
    if (context->device.mode_queues[VULKAN_QUEUE_TYPE_GRAPHICS].family_index != 
        context->device.mode_queues[VULKAN_QUEUE_TYPE_PRESENT].family_index) {
        swapchain_create_info.pQueueFamilyIndices = queueFamilyIndices;
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_create_info.queueFamilyIndexCount = 2;
    }
    
    VkResult result = vkCreateSwapchainKHR(context->device.logical_device, &swapchain_create_info, context->allocator, &out_swapchain->handle);
    if (!vulkan_result_is_success(result)) return result;

    result = vkGetSwapchainImagesKHR(context->device.logical_device, out_swapchain->handle, &out_swapchain->image_count, 0);
    if (!vulkan_result_is_success(result)) return result;

    if (!out_swapchain->images) 
        out_swapchain->images = (VkImage*)ballocate(sizeof(VkImage) * out_swapchain->image_count, MEMORY_TAG_RENDERER);
    
    return vkGetSwapchainImagesKHR(
        context->device.logical_device, 
        out_swapchain->handle, 
        &out_swapchain->image_count, 
        out_swapchain->images);
}

void vulkan_swapchain_destroy(
    vulkan_context* context,
    vulkan_swapchain* swapchain) {
    if (swapchain->handle) {
        vkDeviceWaitIdle(context->device.logical_device);

        bfree(swapchain->images, sizeof(VkImage) * swapchain->image_count, MEMORY_TAG_RENDERER);
        swapchain->images = 0;
        swapchain->image_count = 0;

        vkDestroySwapchainKHR(context->device.logical_device, swapchain->handle, context->allocator);
    }
}

VkResult vulkan_swapchain_acquire_next_image_index(
    vulkan_context* context,
    vulkan_swapchain* swapchain,
    u64 timeout_ns,
    VkSemaphore image_available_semaphore,
    VkFence fence,
    u32* out_image_index) {

    return vkAcquireNextImageKHR(
        context->device.logical_device,
        swapchain->handle,
        timeout_ns,
        image_available_semaphore,
        fence,
        out_image_index);
}

VkResult vulkan_swapchain_present(
    vulkan_context* context,
    vulkan_swapchain* swapchain,
    VkQueue present_queue,
    VkSemaphore render_complete_semaphore,
    u32 present_image_index) {
    // Return the image to the swapchain for presentation.
    VkPresentInfoKHR present_info = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &render_complete_semaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain->handle;
    present_info.pImageIndices = &present_image_index;
    present_info.pResults = 0;

    return vkQueuePresentKHR(present_queue, &present_info);
}