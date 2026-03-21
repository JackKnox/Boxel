#pragma once

#include "defines.h"

#include "vulkan_types.h"

b8 vulkan_rendertarget_create(
    box_renderer_backend* backend,
    box_rendertarget_config* config,
    box_rendertarget* out_rendertarget);

VkResult vulkan_rendertarget_create_internal(
    vulkan_context* context,
    b8 window_dest,
    uvec2 origin, uvec2 size,
    u32 attachment_count,
    vulkan_rendertarget_attachment* attachments,
    box_rendertarget* out_rendertarget);

void vulkan_rendertarget_begin(
    vulkan_context* context,
    vulkan_command_buffer* command_buffer,
    box_rendertarget* rendertarget,
    b8 set_viewport, b8 set_scissor);

void vulkan_rendertarget_end(
    vulkan_context* context,
    vulkan_command_buffer* command_buffer, 
    box_rendertarget* rendertarget);

void vulkan_rendertarget_destroy(
    box_renderer_backend* backend,
    box_rendertarget* rendertarget);