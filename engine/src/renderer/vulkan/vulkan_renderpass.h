#pragma once

#include "defines.h"

#include "vulkan_types.h"

VkResult vulkan_renderpass_create(
    vulkan_context* context,
    vulkan_renderpass* out_renderpass,
    vec2 origin, vec2 size);

void vulkan_renderpass_destroy(vulkan_context* context, vulkan_renderpass* renderpass);

void vulkan_renderpass_begin(
    vulkan_command_buffer* command_buffer,
    vulkan_renderpass* renderpass,
    vulkan_framebuffer* frame_buffer);

void vulkan_renderpass_end(vulkan_command_buffer* command_buffer, vulkan_renderpass* renderpass);