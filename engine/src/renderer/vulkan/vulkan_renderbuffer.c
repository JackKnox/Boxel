#include "defines.h"
#include "vulkan_renderbuffer.h"

#include "vulkan_command_buffer.h"

VkBufferUsageFlags get_vulkan_renderbuffer_usage(
    vulkan_context* context,
	box_renderbuffer* buffer) {
    VkBufferUsageFlags buffer_usage = 0;
	if (context->config.modes & RENDERER_MODE_TRANSFER) buffer_usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	if (buffer->usage & BOX_RENDERBUFFER_USAGE_VERTEX)
		buffer_usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	if (buffer->usage & BOX_RENDERBUFFER_USAGE_INDEX)
		buffer_usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	if (buffer->usage & BOX_RENDERBUFFER_USAGE_STORAGE)
		buffer_usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    
    return buffer_usage;
}

b8 vulkan_renderbuffer_create(
	box_renderer_backend* backend,
	box_renderbuffer* out_buffer) {
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    
    out_buffer->internal_data = ballocate(sizeof(internal_vulkan_renderbuffer), MEMORY_TAG_RENDERER);
    internal_vulkan_renderbuffer* internal_buffer = (internal_vulkan_renderbuffer*)out_buffer->internal_data;

    // TODO: Make configurable.
    internal_buffer->properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VkBufferCreateInfo create_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	create_info.size = out_buffer->buffer_size;
	create_info.usage = get_vulkan_renderbuffer_usage(context, out_buffer);
	create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // TODO: Make configurable.
	VkResult result = vkCreateBuffer(context->device.logical_device, &create_info, context->allocator, &internal_buffer->handle);
	if (!vulkan_result_is_success(result)) return FALSE;

	vkGetBufferMemoryRequirements(context->device.logical_device, internal_buffer->handle, &internal_buffer->memory_requirements);
	i32 memory_index = find_memory_index(context, internal_buffer->memory_requirements.memoryTypeBits, internal_buffer->properties);

    if (memory_index == -1) {
		BX_ERROR("Failed to create Vulkan buffer: Unable to find suitable memory type.");
		return FALSE;
	}

    VkMemoryAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	alloc_info.allocationSize = internal_buffer->memory_requirements.size;
	alloc_info.memoryTypeIndex = memory_index;

	result = vkAllocateMemory(context->device.logical_device, &alloc_info, context->allocator, &internal_buffer->memory);
	if (!vulkan_result_is_success(result)) return FALSE;

    result = vkBindBufferMemory(context->device.logical_device, internal_buffer->handle, internal_buffer->memory, 0);
    if (!vulkan_result_is_success(result)) return FALSE;
    return TRUE;
}

b8 vulkan_renderbuffer_upload_data(
	box_renderer_backend* backend,
	box_renderbuffer* buffer,
	const void* buf_data,
    u64 buf_offset, 
    u64 buf_size) {
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    
    if (!(context->config.modes & RENDERER_MODE_TRANSFER)) {
		BX_ERROR("Attempting to upload to renderbuffer without enabling transfer mode.");
		return FALSE;
	}

    box_renderbuffer staging_buffer = {};
	staging_buffer.buffer_size = buffer->buffer_size;
    if (!create_staging_buffer(context, buf_data, &staging_buffer)) {
        BX_ERROR("Failed to create staging buffer for uploading data.");
        return FALSE;
    }

	vulkan_queue* selected_mode = &context->device.mode_queues[VULKAN_QUEUE_TYPE_TRANSFER];
	vulkan_command_buffer command_buffer;
	vulkan_command_buffer_allocate_and_begin_single_use(context, selected_mode->pool, &command_buffer);

    internal_vulkan_renderbuffer* internal_buffer = (internal_vulkan_renderbuffer*)buffer->internal_data;
    internal_vulkan_renderbuffer* internal_staging_buffer = (internal_vulkan_renderbuffer*)staging_buffer.internal_data;

	VkBufferCopy copy_info = {};
	copy_info.size = buf_size;
	copy_info.dstOffset = buf_offset;
	vkCmdCopyBuffer(command_buffer.handle, internal_staging_buffer->handle, internal_buffer->handle, 1, &copy_info);
    
	CHECK_VKRESULT(
		vulkan_command_buffer_end_single_use(
			context,
			&command_buffer,
			selected_mode->handle),
		"Failed to transfer Vulkan renderbuffer to GPU");
        
	vulkan_renderbuffer_destroy(backend, &staging_buffer);
    return TRUE;
}

void vulkan_renderbuffer_destroy(
	box_renderer_backend* backend,
	box_renderbuffer* buffer) {
    vulkan_context* context = (vulkan_context*)backend->internal_context;

    internal_vulkan_renderbuffer* internal_buffer = (internal_vulkan_renderbuffer*)buffer->internal_data;

	vkDestroyBuffer(context->device.logical_device, internal_buffer->handle, context->allocator);
	vkFreeMemory(context->device.logical_device, internal_buffer->memory, context->allocator);
    bfree(internal_buffer, sizeof(internal_vulkan_renderbuffer), MEMORY_TAG_RENDERER);
}