#pragma once

#include "defines.h"

#include "vulkan_types.h"

// Creates a Vulkan image and optionally an associated image view.
VkResult vulkan_image_create(
    vulkan_context* context,
    uvec2 size,
    VkFormat format,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags memory_flags,
    b8 create_view,
    VkImageAspectFlags view_aspect_flags,
    vulkan_image* out_image);

// Transitions the layout of a Vulkan image through a pipeline barrier.
void vulkan_image_transition_layout(
    vulkan_context* context,
    vulkan_command_buffer* command_buffer, 
    vulkan_image* image,
    VkImageLayout new_layout);

// Copies data from a buffer into an image.
void vulkan_image_copy_from_buffer(
	vulkan_context* context,
	vulkan_command_buffer* command_buffer,
	vulkan_image* image,
	VkBuffer buffer,
    u64 buf_offset,
    uvec2 img_region);

// Destroys a Vulkan image and associated resources.
void vulkan_image_destroy(
    vulkan_context* context,
    vulkan_image* image,
    b8 ownes_image);