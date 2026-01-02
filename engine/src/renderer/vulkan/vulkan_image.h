#pragma once

#include "defines.h"

#include "vulkan_types.h"

VkResult vulkan_image_create(
    vulkan_context* context,
    VkImageType image_type,
    uvec2 size,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags memory_flags,
    b8 create_view,
    VkImageLayout start_layout,
    VkImageAspectFlags view_aspect_flags,
    vulkan_image* out_image);

void vulkan_image_transition_format(
    vulkan_command_buffer* cmd, 
    vulkan_image* image, 
    VkImageLayout new_layout);

VkResult vulkan_image_view_create(
    vulkan_context* context,
    VkFormat format,
    VkImageAspectFlags aspect_flags,
    vulkan_image* out_image);

VkResult vulkan_image_sampler_create(
    vulkan_context* context,
    f32 max_anisotropy,
    box_filter_type filter_type,
    box_address_mode address_mode,
    vulkan_image* out_image);

void vulkan_image_destroy(vulkan_context* context, vulkan_image* image);
