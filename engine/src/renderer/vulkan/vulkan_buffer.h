#pragma once

#include "defines.h"

#include "vulkan_types.h"

VkResult vulkan_buffer_create(
	vulkan_context* context, 
	VkDeviceSize size, 
	VkBufferUsageFlags usage, 
	VkMemoryPropertyFlags properties, 
	vulkan_buffer* out_buffer);

b8 vulkan_buffer_map_data(
	vulkan_context* context,
	vulkan_buffer* buffer,
	VkDeviceSize buf_size,
	void* buf_data);

void vulkan_buffer_destroy(
	vulkan_context* context,
	vulkan_buffer* buffer);