#include "defines.h"
#include "vulkan_image.h"

#include "vulkan_device.h"

VkResult vulkan_image_create(
    vulkan_context* context,
    VkImageType image_type,
    uvec2 size,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags memory_flags,
    b8 create_view,
    b8 create_sampler,
    VkImageLayout start_layout,
    VkImageAspectFlags view_aspect_flags,
    f32 max_anisotropy,
    vulkan_image* out_image) {
    // Copy params
    out_image->size = size;
    out_image->view = 0;
    out_image->layout = start_layout;

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
    image_create_info.initialLayout = out_image->layout;
    image_create_info.usage = usage;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;          // TODO: Configurable sample count.
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;  // TODO: Configurable sharing mode.

    VkResult result = vkCreateImage(context->device.logical_device, &image_create_info, context->allocator, &out_image->handle);
    if (!vulkan_result_is_success(result)) return result;

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
    if (!vulkan_result_is_success(result)) return result;

    // Bind the memory
    result = vkBindImageMemory(context->device.logical_device, out_image->handle, out_image->memory, 0);  // TODO: configurable memory offset.
    if (!vulkan_result_is_success(result)) return result;

    // Create view
    if (create_view)
        result = vulkan_image_view_create(context, out_image, format, view_aspect_flags);

    if (result == VK_SUCCESS && create_sampler)
        result = vulkan_image_sampler_create(context, out_image, max_anisotropy);

    return result;
}

void vulkan_image_transition_format(vulkan_command_buffer* cmd, vulkan_image* image, VkImageLayout new_layout) {
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

    // TODO: Idk what these mean...
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    vkCmdPipelineBarrier(cmd->handle,
        src_stage, 
        dst_stage,
        0,
        0, NULL,
        0, NULL,
        1, &barrier);

    image->layout = new_layout;
}

VkResult vulkan_image_view_create(
    vulkan_context* context,
    vulkan_image* image,
    VkFormat format,
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

    return vkCreateImageView(
        context->device.logical_device,
        &view_create_info,
        context->allocator,
        &image->view);
}

VkResult vulkan_image_sampler_create(
    vulkan_context* context, 
    vulkan_image* out_image,
    f32 max_anisotropy) {
    VkSamplerCreateInfo sampler_info = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    sampler_info.anisotropyEnable = (max_anisotropy > 0);
    sampler_info.maxAnisotropy = max_anisotropy;
    
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.mipLodBias = 0.0f;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = 0.0f;

    return vkCreateSampler(
        context->device.logical_device, 
        &sampler_info,
        context->allocator, 
        &out_image->sampler);
}

void vulkan_image_destroy(vulkan_context* context, vulkan_image* image) {
    if (image->sampler) {
        vkDestroySampler(context->device.logical_device, image->sampler, context->allocator);
        image->sampler = 0;
    }
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