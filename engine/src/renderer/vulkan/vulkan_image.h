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
    b32 create_view,
    VkImageAspectFlags view_aspect_flags,
    vulkan_image* out_image);

VkResult vulkan_image_view_create(
    vulkan_context* context,
    VkFormat format,
    vulkan_image* image,
    VkImageAspectFlags aspect_flags);

void vulkan_image_destroy(vulkan_context* context, vulkan_image* image);
