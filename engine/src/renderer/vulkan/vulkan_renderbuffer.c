#include "defines.h"
#include "vulkan_renderbuffer.h"

#include "vulkan_command_buffer.h"
#include "vulkan_renderbuffer.h"

VkBufferUsageFlags get_vulkan_renderbuffer_usage(
    vulkan_context* context,
	box_renderbuffer_config* config) {
    VkBufferUsageFlags buffer_usage = 0;
	if (context->config.modes & RENDERER_MODE_TRANSFER) buffer_usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	if (config->usage & BOX_RENDERBUFFER_USAGE_VERTEX)
		buffer_usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	if (config->usage & BOX_RENDERBUFFER_USAGE_INDEX)
		buffer_usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	if (config->usage & BOX_RENDERBUFFER_USAGE_STORAGE)
		buffer_usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	if (config->usage & BOX_RENDERBUFFER_USAGE_CPU_VISIBLE)
		buffer_usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    
    return buffer_usage;
}

b8 vulkan_renderbuffer_create(
	box_renderer_backend* backend,
	box_renderbuffer_config* config,
	box_renderbuffer* out_buffer) {
	BX_ASSERT(backend != NULL && config != NULL && out_buffer != NULL && "Invalid arguments passed to vulkan_renderbuffer_create");
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    
#if BOX_ENABLE_VALIDATION
	if (config->buffer_size <= 0) {
		BX_ERROR("vulkan_renderbuffer_create(): Cannot create a render buffer with a size of 0");
		return FALSE;
	}

	if (!config->usage) {
		BX_ERROR("vulkan_renderbuffer_create(): Cannot create a render buffer with no usage set");
		return FALSE;
	}
#endif

    out_buffer->internal_data = ballocate(sizeof(internal_vulkan_renderbuffer), MEMORY_TAG_RENDERER);
    internal_vulkan_renderbuffer* internal_buffer = (internal_vulkan_renderbuffer*)out_buffer->internal_data;

	out_buffer->buffer_size = config->buffer_size;
	
    internal_buffer->properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	if (config->usage & BOX_RENDERBUFFER_USAGE_CPU_VISIBLE)
		internal_buffer->properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    VkBufferCreateInfo create_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	create_info.size = config->buffer_size;
	create_info.usage = get_vulkan_renderbuffer_usage(context, config);
	create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // TODO: Make configurable.
	VkResult result = vkCreateBuffer(context->device.logical_device, &create_info, context->allocator, &internal_buffer->handle);
	if (!vulkan_result_is_success(result)) return FALSE;

	vkGetBufferMemoryRequirements(context->device.logical_device, internal_buffer->handle, &internal_buffer->memory_requirements);
	i32 memory_index = find_memory_index(context, internal_buffer->memory_requirements.memoryTypeBits, internal_buffer->properties);

    if (memory_index == -1) {
		BX_ERROR("vulkan_renderbuffer_create(): Unable to find suitable memory type for renderbuffer.");
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
	BX_ASSERT(backend != NULL && buffer != NULL && buf_data != NULL && "Invalid arguments passed to vulkan_renderbuffer_upload_data");
    vulkan_context* context = (vulkan_context*)backend->internal_context;

#if BOX_ENABLE_VALIDATION
    if (!(context->config.modes & RENDERER_MODE_TRANSFER)) {
		BX_ERROR("vulkan_renderbuffer_upload_data(): Attempting to upload to renderbuffer without enabling transfer mode.");
		return FALSE;
	}

	if (buf_offset + buf_size > buffer->buffer_size) {
		BX_ERROR("vulkan_renderbuffer_upload_data(): Upload range reaches outside of renderbuffer size.");
		return FALSE;
	}
#endif

    internal_vulkan_renderbuffer* internal_buffer = (internal_vulkan_renderbuffer*)buffer->internal_data;

    box_renderbuffer_config staging_buffer_config = box_renderbuffer_default();
	staging_buffer_config.usage = BOX_RENDERBUFFER_USAGE_CPU_VISIBLE;
	staging_buffer_config.buffer_size = buffer->buffer_size;

	box_renderbuffer staging_buffer = {};
    if (!vulkan_renderbuffer_create(backend, &staging_buffer_config, &staging_buffer) || 
		!vulkan_renderbuffer_map_data(backend, &staging_buffer, buf_data, 0, staging_buffer.buffer_size)) {
        BX_ERROR("vulkan_renderbuffer_upload_data(): Failed to create staging buffer for uploading data.");
        return FALSE;
    }

	vulkan_command_buffer command_buffer;
	vulkan_command_buffer_allocate_and_begin_single_use(context, &context->device.mode_queues[VULKAN_QUEUE_TYPE_TRANSFER], &command_buffer);
	{
		internal_vulkan_renderbuffer* internal_staging_buffer = (internal_vulkan_renderbuffer*)staging_buffer.internal_data;

		VkBufferCopy copy_info = {};
		copy_info.size = buf_size;
		copy_info.dstOffset = buf_offset;
		vkCmdCopyBuffer(command_buffer.handle, internal_staging_buffer->handle, internal_buffer->handle, 1, &copy_info);
		
		CHECK_VKRESULT(
			vulkan_command_buffer_end_single_use(
				context,
				&command_buffer),
			"Failed to transfer Vulkan renderbuffer to GPU");
	}
        
	vulkan_renderbuffer_destroy(backend, &staging_buffer);
    return TRUE;
}

b8 vulkan_renderbuffer_map_data(
	box_renderer_backend* backend, 
	box_renderbuffer* buffer, 
	const void* source, 
    u64 buf_offset, 
	u64 buf_size) {
	BX_ASSERT(backend != NULL && buffer != NULL && source != NULL && "Invalid arguments passed to vulkan_renderbuffer_map_data");
    vulkan_context* context = (vulkan_context*)backend->internal_context;

    internal_vulkan_renderbuffer* internal_buffer = (internal_vulkan_renderbuffer*)buffer->internal_data;

    void* map_ptr = NULL;
	CHECK_VKRESULT(
		vkMapMemory(
			context->device.logical_device, 
			internal_buffer->memory, 
			buf_offset, buf_size, 
			0, &map_ptr),
		"Failed to map data to Vulkan buffer");

	bcopy_memory(map_ptr, source, buf_size);
	vkUnmapMemory(context->device.logical_device, internal_buffer->memory);
	return TRUE;
}	

void vulkan_renderbuffer_destroy(
	box_renderer_backend* backend,
	box_renderbuffer* buffer) {
	BX_ASSERT(backend != NULL && buffer != NULL && "Invalid arguments passed to vulkan_renderbuffer_destroy");
    vulkan_context* context = (vulkan_context*)backend->internal_context;
	if (context->device.logical_device) vkDeviceWaitIdle(context->device.logical_device);

    internal_vulkan_renderbuffer* internal_buffer = (internal_vulkan_renderbuffer*)buffer->internal_data;

	if (internal_buffer != NULL) {
		if (internal_buffer->handle)
			vkDestroyBuffer(context->device.logical_device, internal_buffer->handle, context->allocator);

		if (internal_buffer->memory)
			vkFreeMemory(context->device.logical_device, internal_buffer->memory, context->allocator);

		bfree(internal_buffer, sizeof(internal_vulkan_renderbuffer), MEMORY_TAG_RENDERER);
	}
}