#include "defines.h"
#include "vulkan_texture.h"

#include "vulkan_image.h"
#include "vulkan_renderbuffer.h"
#include "vulkan_command_buffer.h"

VkImageUsageFlags get_vulkan_texture_usage(
    vulkan_context* context, 
    box_texture_config* config) {

	VkImageUsageFlags image_usage = 0;
    if (config->usage & BOX_TEXTURE_USAGE_SAMPLED) image_usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (config->usage & BOX_TEXTURE_USAGE_STORAGE) image_usage |= VK_IMAGE_USAGE_STORAGE_BIT;
	if (context->config.modes & RENDERER_MODE_TRANSFER) image_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    return image_usage;
}

b8 vulkan_texture_create(
    box_renderer_backend* backend,
    box_texture_config* config,
    box_texture* out_texture) {
    BX_ASSERT(backend != NULL && out_texture != NULL && "Invalid arguments passed to vulkan_texture_create");
    vulkan_context* context = (vulkan_context*)backend->internal_context;

    out_texture->internal_data = ballocate(sizeof(internal_vulkan_texture), MEMORY_TAG_RENDERER);
    internal_vulkan_texture* internal_texture = (internal_vulkan_texture*)out_texture->internal_data;

    out_texture->image_format = config->image_format;
    out_texture->size = config->size;

#if BOX_ENABLE_VALIDATION
    if (out_texture->size.width <= 0 || out_texture->size.height <= 0) {
        BX_ERROR("vulkan_texture_create(): Texture size must be greater than zero: Size = (%u, %u)", config->size.width, config->size.height);
        return FALSE;
    }

    if (!context->config.sampler_anisotropy && config->max_anisotropy > 0.0f) {
        BX_ERROR("vulkan_texture_create(): Attempting set anisotropy without enabling sampler anisotropy.");
        return FALSE;
    }

    if (config->max_anisotropy > backend->capabilities.max_anisotropy) {
        BX_ERROR("vulkan_texture_create(): Attempting set anisotropy higher than renderer capabilities.");
        return FALSE;
    }
#endif

    CHECK_VKRESULT(
        vulkan_image_create(
            context, 
            out_texture->size, 
            box_render_format_to_vulkan_type(out_texture->image_format), 
            get_vulkan_texture_usage(context, config), 
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
            TRUE, 
            VK_IMAGE_ASPECT_COLOR_BIT, 
            &internal_texture->image),
        "Failed to create internal Vulkan image");

    if (config->usage & BOX_TEXTURE_USAGE_SAMPLED) {
        VkSamplerCreateInfo sampler_info = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        sampler_info.magFilter = sampler_info.minFilter 
            = box_filter_to_vulkan_type(config->filter_type);

        sampler_info.addressModeU = sampler_info.addressModeV = sampler_info.addressModeW 
            = box_address_mode_to_vulkan_type(config->address_mode);

        sampler_info.anisotropyEnable = (config->max_anisotropy > 0.0f);
        sampler_info.maxAnisotropy = config->max_anisotropy;
        
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
    }
    
    // TODO: This feels weird to put this here
	vulkan_command_buffer command_buffer;
	vulkan_command_buffer_allocate_and_begin_single_use(context, &context->device.mode_queues[VULKAN_QUEUE_TYPE_TRANSFER], &command_buffer);
    {
        vulkan_image_transition_layout(context, &command_buffer, &internal_texture->image, VK_IMAGE_LAYOUT_GENERAL);

        CHECK_VKRESULT(
            vulkan_command_buffer_end_single_use(
                context,
                &command_buffer),
            "Failed to transfer Vulkan renderbuffer to GPU");
    }
    
    return TRUE;
}

b8 vulkan_texture_upload_data(
    box_renderer_backend* backend, 
    box_texture* texture, 
    const void* data, 
    uvec2 offset, 
    uvec2 region) {
    BX_ASSERT(backend != NULL && texture != NULL && data != NULL && "Invalid arguments passed to vulkan_texture_upload_data");
    vulkan_context* context = (vulkan_context*)backend->internal_context;

    internal_vulkan_texture* internal_texture = (internal_vulkan_texture*)texture->internal_data;

#if BOX_ENABLE_VALIDATION
	if (!(context->config.modes & RENDERER_MODE_TRANSFER)) {
		BX_ERROR("vulkan_texture_create(): Attempting to upload to texture without enabling transfer mode.");
		return FALSE;
	}

    if (offset.x + region.width > texture->size.width || offset.y + region.height > texture->size.height) {
        BX_ERROR("vulkan_texture_create(): Region size must be within overral texture size: Region = ((%u, %u) -> (%u, %u))", offset.x, offset.y, offset.x + region.width, offset.y + region.height);
        return FALSE;
    }
#endif

    box_renderbuffer_config staging_buffer_config = box_renderbuffer_default();
	staging_buffer_config.usage = BOX_RENDERBUFFER_USAGE_CPU_VISIBLE;
    staging_buffer_config.buffer_size = box_texture_get_size_in_bytes(texture);

    box_renderbuffer staging_buffer = {};
    if (!vulkan_renderbuffer_create(backend, &staging_buffer_config, &staging_buffer) || 
		!vulkan_renderbuffer_map_data(backend, &staging_buffer, data, 0, staging_buffer.buffer_size)) {
        BX_ERROR("vulkan_texture_create(): Failed to create staging buffer for uploading data.");
        return FALSE;
    }

    VkImageLayout old_layout = internal_texture->image.layout;

	vulkan_command_buffer command_buffer;
	vulkan_command_buffer_allocate_and_begin_single_use(context, &context->device.mode_queues[VULKAN_QUEUE_TYPE_TRANSFER], &command_buffer);
    {
        vulkan_image_transition_layout(context, &command_buffer, &internal_texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        internal_vulkan_renderbuffer* internal_staging_buffer = (internal_vulkan_renderbuffer*)staging_buffer.internal_data;
        vulkan_image_copy_from_buffer(context, &command_buffer, &internal_texture->image, internal_staging_buffer->handle, 0, region);

        // TODO: Implicit image transitions.
        vulkan_image_transition_layout(context, &command_buffer, &internal_texture->image, old_layout);

        CHECK_VKRESULT(
            vulkan_command_buffer_end_single_use(
                context,
                &command_buffer),
            "Failed to transfer Vulkan renderbuffer to GPU");
    }

    vulkan_renderbuffer_destroy(backend, &staging_buffer);
    return TRUE;
}

void vulkan_texture_destroy(
    box_renderer_backend* backend, 
    box_texture* texture) {
    BX_ASSERT(backend != NULL && texture != NULL && "Invalid arguments passed to vulkan_texture_destroy");
    vulkan_context* context = (vulkan_context*)backend->internal_context;
	if (context->device.logical_device) vkDeviceWaitIdle(context->device.logical_device);

    internal_vulkan_texture* internal_texture = (internal_vulkan_texture*)texture->internal_data;
    
    if (internal_texture != NULL) {
        if (internal_texture->sampler)
            vkDestroySampler(context->device.logical_device, internal_texture->sampler, context->allocator);

        vulkan_image_destroy(context, &internal_texture->image, TRUE);

        bfree(internal_texture, sizeof(internal_vulkan_texture), MEMORY_TAG_RENDERER);
    }
}