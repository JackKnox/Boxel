#include "defines.h"
#include "vulkan_swapchain.h"

#include "vulkan_device.h"
#include "vulkan_image.h"

VkResult create(vulkan_context* context, VkExtent2D size, vulkan_swapchain* swapchain);
void destroy(vulkan_context* context, vulkan_swapchain* swapchain);

VkResult vulkan_swapchain_create(
    vulkan_context* context,
    u32 width,
    u32 height,
    vulkan_swapchain* out_swapchain) {
    // Simply create a new one.
    return create(context, (VkExtent2D) { width, height }, out_swapchain);
}

VkResult vulkan_swapchain_recreate(
    vulkan_context* context,
    u32 width,
    u32 height,
    vulkan_swapchain* swapchain) {
    // Destroy the old and create a new one.
    destroy(context, swapchain);
    return create(context, (VkExtent2D) { width, height }, swapchain);
}

void vulkan_swapchain_destroy(
    vulkan_context* context,
    vulkan_swapchain* swapchain) {
    destroy(context, swapchain);
}

b8 vulkan_swapchain_acquire_next_image_index(
    vulkan_context* context,
    vulkan_swapchain* swapchain,
    u64 timeout_ns,
    VkSemaphore image_available_semaphore,
    VkFence fence,
    u32* out_image_index) {

    VkResult result = vkAcquireNextImageKHR(
        context->device.logical_device,
        swapchain->handle,
        timeout_ns,
        image_available_semaphore,
        fence,
        out_image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Trigger swapchain recreation, then boot out of the render loop.
        if (!vulkan_swapchain_recreate(context, context->config->framebuffer_width, context->config->framebuffer_height, swapchain)) {
            BX_ERROR("Failed to recreate swapchain!");
        }

        return FALSE;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        BX_ERROR("Failed to acquire swapchain image!");
        return FALSE;
    }

    return TRUE;
}

void vulkan_swapchain_present(
    vulkan_context* context,
    vulkan_swapchain* swapchain,
    VkQueue graphics_queue,
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

    VkResult result = vkQueuePresentKHR(present_queue, &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        // Swapchain is out of date, suboptimal or a framebuffer resize has occurred. Trigger swapchain recreation.
        vulkan_swapchain_recreate(context, context->config->framebuffer_width, context->config->framebuffer_height, swapchain);
    } else if (result != VK_SUCCESS) {
        BX_ERROR("Failed to present swap chain image!");
    }
}

VkSurfaceFormatKHR find_swapchain_format(vulkan_swapchain_support_info* swapchain_info) {
    for (u32 i = 0; i < swapchain_info->format_count; ++i) {
        VkSurfaceFormatKHR format = swapchain_info->formats[i];
        // Preferred formats
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    return swapchain_info->formats[0];
}

VkResult create(vulkan_context* context, VkExtent2D size, vulkan_swapchain* swapchain) {
    if (size.width == 0 || size.height == 0) {
        BX_ERROR("Could not create swapchain with a size of 0");
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    // Choose a swap surface format.
    swapchain->image_format = find_swapchain_format(&context->device.swapchain_support);
    swapchain->max_frames_in_flight = context->config->swapchain_frame_count;
    
    // Requery swapchain support.
    vulkan_device_query_swapchain_support(
        context->device.physical_device,
        context->surface,
        &context->device.swapchain_support);

    // Swapchain extent
    if (context->device.swapchain_support.capabilities.currentExtent.width != UINT32_MAX) {
        size = context->device.swapchain_support.capabilities.currentExtent;
    }

    // Clamp to the value allowed by the GPU.
    VkExtent2D min = context->device.swapchain_support.capabilities.minImageExtent;
    VkExtent2D max = context->device.swapchain_support.capabilities.maxImageExtent;
    size.width = BX_CLAMP(size.width, min.width, max.width);
    size.height = BX_CLAMP(size.height, min.height, max.height);

    u32 image_count = context->device.swapchain_support.capabilities.minImageCount + 1;
    if (context->device.swapchain_support.capabilities.maxImageCount > 0 && image_count > context->device.swapchain_support.capabilities.maxImageCount) {
        image_count = context->device.swapchain_support.capabilities.maxImageCount;
    }

    // Swapchain create info
    VkSwapchainCreateInfoKHR swapchain_create_info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    swapchain_create_info.surface = context->surface;
    swapchain_create_info.minImageCount = image_count;
    swapchain_create_info.imageFormat = swapchain->image_format.format;
    swapchain_create_info.imageColorSpace = swapchain->image_format.colorSpace;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_create_info.imageExtent = size;
    swapchain_create_info.imageArrayLayers = 1;

    // Setup the queue family indices
    if (context->config->capabilities.graphics_queue_index != context->config->capabilities.present_queue_index) {
        u32 queueFamilyIndices[] = {
            (u32)context->config->capabilities.graphics_queue_index,
            (u32)context->config->capabilities.present_queue_index};
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_create_info.queueFamilyIndexCount = 2;
        swapchain_create_info.pQueueFamilyIndices = queueFamilyIndices;
    } 
    else {
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchain_create_info.queueFamilyIndexCount = 0;
        swapchain_create_info.pQueueFamilyIndices = 0;
    }

    swapchain_create_info.preTransform = context->device.swapchain_support.capabilities.currentTransform;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchain_create_info.clipped = VK_TRUE;
    swapchain_create_info.oldSwapchain = 0;

    VkResult result = vkCreateSwapchainKHR(context->device.logical_device, &swapchain_create_info, context->allocator, &swapchain->handle);
    if (!vulkan_result_is_success(result)) return result;

    // Images
    swapchain->image_count = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(context->device.logical_device, swapchain->handle, &swapchain->image_count, 0));
    if (!swapchain->images) {
        swapchain->images = (VkImage*)ballocate(sizeof(VkImage) * swapchain->image_count, MEMORY_TAG_RENDERER);
    }
    if (!swapchain->views) {
        swapchain->views = (VkImageView*)ballocate(sizeof(VkImageView) * swapchain->image_count, MEMORY_TAG_RENDERER);
    }
    VK_CHECK(vkGetSwapchainImagesKHR(context->device.logical_device, swapchain->handle, &swapchain->image_count, swapchain->images));

    // Views
    for (u32 i = 0; i < swapchain->image_count; ++i) {
        VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view_info.image = swapchain->images[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = swapchain->image_format.format;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        result = vkCreateImageView(context->device.logical_device, &view_info, context->allocator, &swapchain->views[i]);
        if (!vulkan_result_is_success(result)) return result;
    }

    // Create depth image and its view.
    result = vulkan_image_create(
        context,
        VK_IMAGE_TYPE_2D,
        (uvec2) { size.width, size.height },
        context->device.depth_format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        TRUE,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        &swapchain->depth_attachment);
    if (!vulkan_result_is_success(result)) return result;

    BX_INFO("Swapchain created successfully.");
    return VK_SUCCESS;
}

void destroy(vulkan_context* context, vulkan_swapchain* swapchain) {
    if (swapchain && swapchain->handle) {
        vkDeviceWaitIdle(context->device.logical_device);
        vulkan_image_destroy(context, &swapchain->depth_attachment);

        // Only destroy the views, not the images, since those are owned by the swapchain and are thus
        // destroyed when it is.
        for (u32 i = 0; i < swapchain->image_count; ++i) {
            vkDestroyImageView(context->device.logical_device, swapchain->views[i], context->allocator);
        }

        bfree(swapchain->images, sizeof(VkImage) * swapchain->image_count, MEMORY_TAG_RENDERER);
        bfree(swapchain->views, sizeof(VkImageView) * swapchain->image_count, MEMORY_TAG_RENDERER);

        vkDestroySwapchainKHR(context->device.logical_device, swapchain->handle, context->allocator);
    }
}