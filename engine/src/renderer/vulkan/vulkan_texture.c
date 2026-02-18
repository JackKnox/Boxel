#include "defines.h"
#include "vulkan_texture.h"

#include "vulkan_renderbuffer.h"
#include "vulkan_command_buffer.h"

VkImageUsageFlags get_vulkan_texture_usage(
    vulkan_context* context, 
    box_texture* texture) {

	VkImageUsageFlags image_usage = VK_IMAGE_USAGE_SAMPLED_BIT;
	if (context->config.modes & RENDERER_MODE_TRANSFER) image_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    return image_usage;
}

b8 vulkan_texture_create(
    box_renderer_backend* backend, 
    box_texture* out_texture) {
    vulkan_context* context = (vulkan_context*)backend->internal_context;

    if (!context->config.sampler_anisotropy && out_texture->max_anisotropy > 0.0f) {
        BX_ERROR("Attempting set anisotropy without enabling sampler anisotropy.");
        return FALSE;
    }

    if (out_texture->max_anisotropy > backend->capabilities.max_anisotropy) {
        BX_ERROR("Attempting set anisotropy higher than renderer capabilities.");
        return FALSE;
    }

    out_texture->internal_data = ballocate(sizeof(internal_vulkan_texture), MEMORY_TAG_RENDERER);
    internal_vulkan_texture* internal_texture = (internal_vulkan_texture*)out_texture->internal_data;

    VkImageCreateInfo image_create_info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    image_create_info.imageType = VK_IMAGE_TYPE_2D; // TODO: Make configurable.
    image_create_info.extent.width = out_texture->size.width;
    image_create_info.extent.height = out_texture->size.height;
    image_create_info.extent.depth = 1;  // TODO: Support configurable depth.
    image_create_info.mipLevels = 1;     // TODO: Support mip mapping
    image_create_info.arrayLayers = 1;   // TODO: Support number of layers in the image.
    image_create_info.format = box_format_to_vulkan_type(out_texture->image_format);
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_create_info.usage = get_vulkan_texture_usage(context, out_texture); // TODO: Make configurable.
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;          // TODO: Configurable sample count.
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;  // TODO: Configurable sharing mode.

    CHECK_VKRESULT(
        vkCreateImage(
            context->device.logical_device,
            &image_create_info, 
            context->allocator,
            &internal_texture->handle),
        "Failed to create internal Vulkan image");

    // Query memory requirements.
    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(context->device.logical_device, internal_texture->handle, &memory_requirements);

    i32 memory_index = find_memory_index(context, memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memory_index == -1) {
        BX_ERROR("Required memory type not found. Image not valid.");
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    // Allocate memory
    VkMemoryAllocateInfo memory_allocate_info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    memory_allocate_info.allocationSize = memory_requirements.size;
    memory_allocate_info.memoryTypeIndex = memory_index;

    VkResult result = vkAllocateMemory(context->device.logical_device, &memory_allocate_info, context->allocator, &internal_texture->memory);
    if (!vulkan_result_is_success(result)) return FALSE;

    // Bind the memory
    result = vkBindImageMemory(context->device.logical_device, internal_texture->handle, internal_texture->memory, 0);
    if (!vulkan_result_is_success(result)) return FALSE;

    VkImageViewCreateInfo view_create_info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    view_create_info.image = internal_texture->handle;
    view_create_info.viewType = image_create_info.imageType;
    view_create_info.format = image_create_info.format;
    view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    // TODO: Make configurable
    view_create_info.subresourceRange.baseMipLevel = 0;
    view_create_info.subresourceRange.levelCount = 1;
    view_create_info.subresourceRange.baseArrayLayer = 0;
    view_create_info.subresourceRange.layerCount = 1;

    result = vkCreateImageView(context->device.logical_device, &view_create_info, context->allocator, &internal_texture->view);
    if (!vulkan_result_is_success(result)) return FALSE;

    VkSamplerCreateInfo sampler_info = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    sampler_info.magFilter = sampler_info.minFilter 
        = box_filter_to_vulkan_type(out_texture->filter_type);

    sampler_info.addressModeU = sampler_info.addressModeV = sampler_info.addressModeW 
        = box_address_mode_to_vulkan_type(out_texture->address_mode);

    sampler_info.anisotropyEnable = (out_texture->max_anisotropy > 0.0f);
    sampler_info.maxAnisotropy = out_texture->max_anisotropy;
    
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.mipLodBias = 0.0f;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = 0.0f;

    CHECK_VKRESULT(
        vkCreateSampler(
            context->device.logical_device, 
            &sampler_info,
            context->allocator, 
            &internal_texture->sampler),
        "Failed to create internal Vulkan sampler");
    
    return TRUE;
}

b8 vulkan_texture_upload_data(
    box_renderer_backend* backend, 
    box_texture* texture, 
    const void* data, 
    uvec2 offset, 
    uvec2 region) {
    vulkan_context* context = (vulkan_context*)backend->internal_context;

#if BOX_ENABLE_VALIDATION
	if (!(context->config.modes & RENDERER_MODE_TRANSFER)) {
		BX_ERROR("Attempting to upload to texture without enabling transfer mode.");
		return FALSE;
	}
#endif

    internal_vulkan_texture* internal_texture = (internal_vulkan_texture*)texture->internal_data;

    box_renderbuffer staging_buffer = {};
    staging_buffer.buffer_size = box_texture_get_size_in_bytes(texture);
    if (!create_staging_buffer(context, data, &staging_buffer)) {
        BX_ERROR("Failed to create staging buffer for uploading data.");
        return FALSE;
    }

    vulkan_queue* selected_mode = &context->device.mode_queues[VULKAN_QUEUE_TYPE_TRANSFER];
	vulkan_command_buffer command_buffer;
	vulkan_command_buffer_allocate_and_begin_single_use(context, selected_mode->pool, &command_buffer);

	vulkan_texture_transition_layout(backend, &command_buffer, texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    internal_vulkan_renderbuffer* internal_staging_buffer = (internal_vulkan_renderbuffer*)staging_buffer.internal_data;

	VkBufferImageCopy copy_info = { 0 };
	copy_info.imageOffset = (VkOffset3D) { offset.x, offset.y, 0 };
	copy_info.imageExtent = (VkExtent3D){ region.x, region.y, 1 };
	copy_info.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy_info.imageSubresource.layerCount = 1;
	vkCmdCopyBufferToImage(command_buffer.handle, internal_staging_buffer->handle, internal_texture->handle, internal_texture->layout, 1, &copy_info);

    // TODO: Implicit image transitions.
	vulkan_texture_transition_layout(backend, &command_buffer, texture, VK_IMAGE_LAYOUT_GENERAL);

	CHECK_VKRESULT(
		vulkan_command_buffer_end_single_use(
			context,
			&command_buffer,
			selected_mode->handle),
		"Failed to transfer Vulkan renderbuffer to GPU");

    vulkan_renderbuffer_destroy(backend, &staging_buffer);
    return TRUE;
}

void vulkan_texture_transition_layout(
    box_renderer_backend* backend,
    vulkan_command_buffer* cmd, 
    box_texture* texture, 
    VkImageLayout new_layout) {
    vulkan_context* context = (vulkan_context*)backend->internal_context;

    internal_vulkan_texture* internal_texture = (internal_vulkan_texture*)texture->internal_data;
    if (internal_texture->layout == new_layout) return;

    VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.oldLayout = internal_texture->layout;
    barrier.newLayout = new_layout;
    barrier.image = internal_texture->handle;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

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

    internal_texture->layout = new_layout;
}

void vulkan_texture_destroy(
    box_renderer_backend *backend, 
    box_texture *texture) {
    vulkan_context* context = (vulkan_context*)backend->internal_context;

    internal_vulkan_texture* internal_texture = (internal_vulkan_texture*)texture->internal_data;

    vkDestroySampler(context->device.logical_device, internal_texture->sampler, context->allocator);
    vkDestroyImageView(context->device.logical_device, internal_texture->view, context->allocator);
    vkFreeMemory(context->device.logical_device, internal_texture->memory, context->allocator);
    vkDestroyImage(context->device.logical_device, internal_texture->handle, context->allocator);
    bfree(internal_texture, sizeof(internal_vulkan_texture), MEMORY_TAG_RENDERER);
}