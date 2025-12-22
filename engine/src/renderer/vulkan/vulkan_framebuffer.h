#pragma once

#include "defines.h"

#include "vulkan_types.h"

VkResult vulkan_framebuffer_create(
    vulkan_context* context,
    vulkan_renderpass* renderpass,
    vec2 size,
    u32 attachment_count,
    VkImageView* attachments,
    vulkan_framebuffer* out_framebuffer);

void vulkan_framebuffer_destroy(vulkan_context* context, vulkan_framebuffer* framebuffer);
