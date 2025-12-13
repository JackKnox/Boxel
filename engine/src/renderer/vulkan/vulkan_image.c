#include "defines.h"
#include "vulkan_image.h"

#include "vulkan_device.h"

b8 vulkan_image_create(
    vulkan_context* context,
    VkImageType image_type,
    uvec2 size,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags memory_flags,
    b32 create_view,
    VkImageAspectFlags view_aspect_flags,
    vulkan_image* out_image) {
    // Copy params
    out_image->size = size;
    out_image->view = 0;

    // Creation info.
    VkImageCreateInfo image_create_info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    image_create_info.imageType = image_type;
    image_create_info.extent.width = out_image->size.x;
    image_create_info.extent.height = out_image->size.y;
    image_create_info.extent.depth = 1;  // TODO: Support configurable depth.
    image_create_info.mipLevels = 4;     // TODO: Support mip mapping
    image_create_info.arrayLayers = 1;   // TODO: Support number of layers in the image.
    image_create_info.format = format;
    image_create_info.tiling = tiling;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_create_info.usage = usage;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;          // TODO: Configurable sample count.
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;  // TODO: Configurable sharing mode.

    VkResult result = vkCreateImage(context->device.logical_device, &image_create_info, context->allocator, &out_image->handle);
    if (!vulkan_result_is_success(result)) {
        BX_ERROR("Failed to create Vulkan image: %s", vulkan_result_string(result, FALSE));
        return FALSE;
    }

    // Query memory requirements.
    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(context->device.logical_device, out_image->handle, &memory_requirements);

    i32 memory_index = find_memory_index(context, memory_requirements.memoryTypeBits, memory_flags);
    if (memory_index == -1) {
        BX_ERROR("Required memory type not found. Image not valid.");
        return FALSE;
    }

    // Allocate memory
    VkMemoryAllocateInfo memory_allocate_info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    memory_allocate_info.allocationSize = memory_requirements.size;
    memory_allocate_info.memoryTypeIndex = memory_index;

    result = vkAllocateMemory(context->device.logical_device, &memory_allocate_info, context->allocator, &out_image->memory);
    if (!vulkan_result_is_success(result)) {
        BX_ERROR("Failed to create Vulkan image: %s", vulkan_result_string(result, FALSE));
        return FALSE;
    }

    // Bind the memory
    result = vkBindImageMemory(context->device.logical_device, out_image->handle, out_image->memory, 0);  // TODO: configurable memory offset.
    if (!vulkan_result_is_success(result)) {
        BX_ERROR("Failed to create Vulkan image: %s", vulkan_result_string(result, FALSE));
        return FALSE;
    }

    // Create view
    if (create_view)
        return vulkan_image_view_create(context, format, out_image, view_aspect_flags);

    return TRUE;
}

b8 vulkan_image_view_create(
    vulkan_context* context,
    VkFormat format,
    vulkan_image* image,
    VkImageAspectFlags aspect_flags) {
    VkImageViewCreateInfo view_create_info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    view_create_info.image = image->handle;
    view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;  // TODO: Make configurable.
    view_create_info.format = format;
    view_create_info.subresourceRange.aspectMask = aspect_flags;

    // TODO: Make configurable
    view_create_info.subresourceRange.baseMipLevel = 0;
    view_create_info.subresourceRange.levelCount = 1;
    view_create_info.subresourceRange.baseArrayLayer = 0;
    view_create_info.subresourceRange.layerCount = 1;

    return vulkan_result_is_success(
        vkCreateImageView(
            context->device.logical_device,
            &view_create_info,
            context->allocator,
            &image->view));
}

void vulkan_image_destroy(vulkan_context* context, vulkan_image* image) {
    if (image->view) {
        vkDestroyImageView(context->device.logical_device, image->view, context->allocator);
        image->view = 0;
    }
    if (image->memory) {
        vkFreeMemory(context->device.logical_device, image->memory, context->allocator);
        image->memory = 0;
    }
    if (image->handle) {
        vkDestroyImage(context->device.logical_device, image->handle, context->allocator);
        image->handle = 0;
    }
}