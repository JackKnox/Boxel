#include "defines.h"
#include "vulkan_image.h"

VkResult vulkan_image_create(
    vulkan_context* context, 
    uvec2 size, 
    VkFormat format, 
    VkImageUsageFlags usage, 
    VkMemoryPropertyFlags memory_flags, 
    b8 create_view, 
    VkImageAspectFlags view_aspect_flags, 
    vulkan_image* out_image) {
    VkImageCreateInfo image_create_info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.extent.width = size.width;
    image_create_info.extent.height = size.height;
    image_create_info.extent.depth = 1;
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.format = format;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_create_info.usage = usage;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkResult result = vkCreateImage(context->device.logical_device, &image_create_info, context->allocator, &out_image->handle);
    if (!vulkan_result_is_success(result)) return FALSE;
    
    // Query memory requirements.
    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(context->device.logical_device, out_image->handle, &memory_requirements);

    i32 memory_index = find_memory_index(context, memory_requirements.memoryTypeBits, memory_flags);
    if (memory_index == -1) {
        BX_ERROR("Required memory type not found. Image not valid.");
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    // Allocate memory
    VkMemoryAllocateInfo memory_allocate_info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    memory_allocate_info.allocationSize = memory_requirements.size;
    memory_allocate_info.memoryTypeIndex = memory_index;

    result = vkAllocateMemory(context->device.logical_device, &memory_allocate_info, context->allocator, &out_image->memory);
    if (!vulkan_result_is_success(result)) return FALSE;

    // Bind the memory
    result = vkBindImageMemory(context->device.logical_device, out_image->handle, out_image->memory, 0);
    if (!vulkan_result_is_success(result)) return FALSE;

    VkImageViewCreateInfo view_create_info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    view_create_info.image = out_image->handle;
    view_create_info.viewType = image_create_info.imageType;
    view_create_info.format = image_create_info.format;
    view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    // TODO: Make configurable
    view_create_info.subresourceRange.baseMipLevel = 0;
    view_create_info.subresourceRange.levelCount = 1;
    view_create_info.subresourceRange.baseArrayLayer = 0;
    view_create_info.subresourceRange.layerCount = 1;

    return vkCreateImageView(
        context->device.logical_device, 
        &view_create_info, 
        context->allocator, 
        &out_image->view);
}

void vulkan_image_transition_layout(
    vulkan_context* context, 
    vulkan_command_buffer* command_buffer, 
    vulkan_image* image, 
    VkImageLayout new_layout) {
    if (image->layout == new_layout) return;

    VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.oldLayout = image->layout;
    barrier.newLayout = new_layout;
    barrier.image = image->handle;
    
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    vkCmdPipelineBarrier(command_buffer->handle,
        src_stage, 
        dst_stage,
        0,
        0, NULL,
        0, NULL,
        1, &barrier);

    image->layout = new_layout;
}

void vulkan_image_copy_from_buffer(
    vulkan_context* context, 
    vulkan_command_buffer* command_buffer, 
    vulkan_image* image, 
    VkBuffer buffer,
    u64 buf_offset,
    uvec2 img_region) {
    VkBufferImageCopy region = {};
    region.bufferOffset = buf_offset;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

    region.imageExtent.width = img_region.width;
    region.imageExtent.height = img_region.height;
    region.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(command_buffer->handle, buffer, image->handle, image->layout, 1, &region);
}

void vulkan_image_destroy(
    vulkan_context* context, 
    vulkan_image* image,
    b8 ownes_image) {
    if (image != NULL) {
        if (image->view)
            vkDestroyImageView(context->device.logical_device, image->view, context->allocator);
        
        if (image->memory && ownes_image)
            vkFreeMemory(context->device.logical_device, image->memory, context->allocator);

        if (image->handle && ownes_image)
            vkDestroyImage(context->device.logical_device, image->handle, context->allocator);
    }
}
