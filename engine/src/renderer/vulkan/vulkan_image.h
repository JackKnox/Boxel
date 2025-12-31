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
    b8 create_sampler,
    VkImageLayout start_layout,
    VkImageAspectFlags view_aspect_flags,
    f32 max_anisotropy,
    vulkan_image* out_image);

void vulkan_image_transition_format(
    vulkan_command_buffer* cmd, 
    vulkan_image* image, 
    VkImageLayout new_layout);

VkResult vulkan_image_view_create(
    vulkan_context* context,
    vulkan_image* out_image,
    VkFormat format,
    VkImageAspectFlags aspect_flags);

VkResult vulkan_image_sampler_create(
    vulkan_context* context,
    vulkan_image* out_image,
    f32 max_anisotropy);

void vulkan_image_destroy(vulkan_context* context, vulkan_image* image);
