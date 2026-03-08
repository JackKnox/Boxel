#pragma once

#include "defines.h"

#include "vulkan_types.h"

b8 vulkan_rendertarget_create(
    box_renderer_backend* backend,
    box_rendertarget* out_rendertarget);

b8 vulkan_rendertarget_create_external(
    vulkan_context* context,
    vulkan_image* external_images,
    u32 image_count,
    box_rendertarget* out_rendertarget);

void vulkan_rendertarget_begin(
    box_renderer_backend* backend,
    vulkan_command_buffer* command_buffer,
    box_rendertarget* rendertarget,
    b8 set_viewport, b8 set_scissor);

void vulkan_rendertarget_end(
    box_renderer_backend* backend,
    vulkan_command_buffer* command_buffer, 
    box_rendertarget* rendertarget);

void vulkan_rendertarget_destroy(
    box_renderer_backend* backend,
    box_rendertarget* rendertarget);