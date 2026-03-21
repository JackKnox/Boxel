#include "defines.h"
#include "vulkan_window_system.h"

#include "utils/darray.h"

#include "vulkan_rendertarget.h"
#include "vulkan_image.h"

VkResult query_support_info(
    vulkan_context* context, 
    vulkan_window_system* window_system) {
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

    window_system->present_modes = darray_reserve(VkPresentModeKHR, present_mode_count, MEMORY_TAG_RENDERER);
    vkGetPhysicalDeviceSurfacePresentModesKHR(context->device.physical_device, window_system->surface, &present_mode_count, window_system->present_modes);
    darray_length_set(window_system->present_modes, present_mode_count);
    return VK_SUCCESS;
}

VkResult vulkan_window_system_create(
    box_renderer_backend* backend,
    vulkan_window_system* out_window_system) {
    BX_ASSERT(backend->platform != NULL && "Vulkan window system created without attachted platform state");
    vulkan_context* context = (vulkan_context*)backend->internal_context;

    VkResult result = vulkan_platform_create_surface(
            context->instance, 
            backend->platform,
            context->allocator, 
            &out_window_system->surface);
    if (!vulkan_result_is_success(result)) return result;

    result = query_support_info(context, out_window_system);
    if (!vulkan_result_is_success(result)) return result;
    
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
    //window_size.width = BX_CLAMP(window_size.width, min.width, max.width);
    //window_size.height = BX_CLAMP(window_size.height, min.height, max.height);

    u32 image_count = out_window_system->capabilities.minImageCount + 1;
    if (out_window_system->capabilities.maxImageCount > 0)
        image_count = BX_MIN(image_count, out_window_system->capabilities.maxImageCount);

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
    swapchain_create_info.imageExtent = (VkExtent2D) { context->config.starting_size.width, context->config.starting_size.height };
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.clipped = TRUE;

    // Setup the queue family indices
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (context->device.mode_queues[VULKAN_QUEUE_TYPE_GRAPHICS].family_index != 
        context->device.mode_queues[VULKAN_QUEUE_TYPE_PRESENT].family_index) {
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_create_info.pQueueFamilyIndices = queueFamilyIndices;
        swapchain_create_info.queueFamilyIndexCount = 2;
    }

    result = vkCreateSwapchainKHR(
            context->device.logical_device,
            &swapchain_create_info,
            context->allocator,
            &out_window_system->swapchain);
    if (!vulkan_result_is_success(result)) return result;

    result = vkGetSwapchainImagesKHR(
            context->device.logical_device, 
            out_window_system->swapchain, 
            &out_window_system->image_count,
            0);
    if (!vulkan_result_is_success(result)) return result;

    VkImage* images = (VkImage*)ballocate(sizeof(VkImage) * out_window_system->image_count, MEMORY_TAG_RENDERER);
    vkGetSwapchainImagesKHR(context->device.logical_device, out_window_system->swapchain, &out_window_system->image_count, images);

    out_window_system->images = (vulkan_image*)ballocate(sizeof(vulkan_image) * out_window_system->image_count, MEMORY_TAG_RENDERER);

    for (u32 i = 0; i < out_window_system->image_count; ++i) {
        out_window_system->images[i].handle = images[i];

        VkImageViewCreateInfo view_info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        view_info.image = out_window_system->images[i].handle;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = out_window_system->swapchain_format.format;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        result = vkCreateImageView(
                context->device.logical_device, 
                &view_info, 
                context->allocator, 
                &out_window_system->images[i].view);
        if (!vulkan_result_is_success(result)) return result;
    }

    bfree(images, sizeof(VkImage) * out_window_system->image_count, MEMORY_TAG_RENDERER);

	out_window_system->images_in_flight = darray_reserve(VkFence*, out_window_system->image_count, MEMORY_TAG_RENDERER);
	out_window_system->image_available_semaphores = darray_reserve(VkSemaphore, context->config.frames_in_flight, MEMORY_TAG_RENDERER);

    for (u32 i = 0; i < context->config.frames_in_flight; ++i) {
		VkSemaphoreCreateInfo semaphore_create_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        
        VkSemaphore* semaphore = darray_push_empty(out_window_system->image_available_semaphores);

        result = vkCreateSemaphore(
            context->device.logical_device, 
            &semaphore_create_info, 
            context->allocator, 
            semaphore);
        if (!vulkan_result_is_success(result)) return result;
    }

    return TRUE;
}

void vulkan_window_system_destroy(
    box_renderer_backend* backend, 
    vulkan_window_system* window_system) {
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    
    for (u32 i = 0; i < darray_length(window_system->image_available_semaphores); ++i) {
        if (!window_system->image_available_semaphores[i]) continue;

        vkDestroySemaphore(
            context->device.logical_device, 
            window_system->image_available_semaphores[i], 
            context->allocator);
    }

    darray_destroy(window_system->image_available_semaphores);
    darray_destroy(window_system->images_in_flight);

    bfree(window_system->images, 
        sizeof(vulkan_image) * window_system->image_count, 
        MEMORY_TAG_RENDERER);

    vkDestroySwapchainKHR(context->device.logical_device, window_system->swapchain, context->allocator);

    darray_destroy(window_system->formats);
    darray_destroy(window_system->present_modes);

    vkDestroySurfaceKHR(context->instance, window_system->surface, context->allocator);
}

VkResult vulkan_window_system_acquire(
    box_renderer_backend* backend, 
    vulkan_window_system* window_system, 
    u64 timeout, 
    VkFence wait_fence) {
    vulkan_context* context = (vulkan_context*)backend->internal_context;

    VkResult result = vkAcquireNextImageKHR(
            context->device.logical_device,
            window_system->swapchain,
            timeout,
            window_system->image_available_semaphores[context->current_frame],
            wait_fence,
            &context->image_index);
    if (!vulkan_result_is_success(result)) return result;
    
	/*
    // If this swapchain image is still in flight, wait for it
	if (window_system->images_in_flight[context->image_index] != VK_NULL_HANDLE) {
        result = vkWaitForFences(
            context->device.logical_device, 
            1, 
            window_system->images_in_flight[context->image_index], 
            TRUE, 
            UINT64_MAX);
        if (!vulkan_result_is_success(result)) return result;
    }

	window_system->images_in_flight[context->image_index] = &context->in_flight_fences[context->current_frame];
    */
    return VK_SUCCESS;
}

VkResult vulkan_window_system_present(
    box_renderer_backend* backend, 
    vulkan_window_system* window_system, 
    vulkan_queue* present_queue, 
    u32 wait_semaphore_count, 
    VkSemaphore* wait_semaphores) {
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    
    VkResult present_result = VK_SUCCESS;

    VkPresentInfoKHR present_info = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    present_info.waitSemaphoreCount = wait_semaphore_count;
    present_info.pWaitSemaphores = wait_semaphores;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &window_system->swapchain;
    present_info.pImageIndices = &context->image_index;
    present_info.pResults = &present_result;

    VkResult result = vkQueuePresentKHR(
            present_queue->handle, 
            &present_info);
    if (!vulkan_result_is_success(result)) return result;

    return present_result;
}