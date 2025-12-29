#include "defines.h"
#include "vulkan_buffer.h"

VkResult vulkan_buffer_create(
	vulkan_context* context, 
	VkDeviceSize size, 
	VkBufferUsageFlags usage, 
	VkMemoryPropertyFlags properties, 
	vulkan_buffer* out_buffer) {
	out_buffer->usage = usage;
	out_buffer->properties = properties;

	VkBufferCreateInfo create_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	create_info.size = size;
	create_info.usage = usage;
	create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // TODO: Make configurable.

	VkResult result = vkCreateBuffer(context->device.logical_device, &create_info, context->allocator, &out_buffer->handle);
	if (!vulkan_result_is_success(result)) return result;

	vkGetBufferMemoryRequirements(context->device.logical_device, out_buffer->handle, &out_buffer->memory_requirements);
	i32 memory_index = find_memory_index(context, out_buffer->memory_requirements.memoryTypeBits, out_buffer->properties);

	if (memory_index == -1) {
		BX_ERROR("Failed to create Vulkan buffer: Unable to find suitable memory type.");
		return FALSE;
	}

	VkMemoryAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	alloc_info.allocationSize = out_buffer->memory_requirements.size;
	alloc_info.memoryTypeIndex = memory_index;

	result = vkAllocateMemory(context->device.logical_device, &alloc_info, context->allocator, &out_buffer->memory);
	if (!vulkan_result_is_success(result)) return result;

	return vkBindBufferMemory(
		context->device.logical_device,
		out_buffer->handle,
		out_buffer->memory,
		0);
		
}

b8 vulkan_buffer_map_data(vulkan_context* context, vulkan_buffer* buffer, VkDeviceSize buf_size, void* buf_data) {
	void* data;
	if (!vulkan_result_is_success(vkMapMemory(context->device.logical_device, buffer->memory, 0, buf_size, 0, &data))) {
		return FALSE;
	}

	bcopy_memory(data, buf_data, buf_size);
	vkUnmapMemory(context->device.logical_device, buffer->memory);
	return TRUE;
}

void vulkan_buffer_destroy(
	vulkan_context* context, 
	vulkan_buffer* buffer) {
	vkDestroyBuffer(context->device.logical_device, buffer->handle, NULL);
	vkFreeMemory(context->device.logical_device, buffer->memory, NULL);
}
