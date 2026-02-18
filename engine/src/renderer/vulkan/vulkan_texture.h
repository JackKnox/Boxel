#pragma once

#include "defines.h"

#include "vulkan_types.h"

b8 vulkan_texture_create(
	box_renderer_backend* backend,
    box_texture* out_texture);

b8 vulkan_texture_upload_data(
    box_renderer_backend* backend, 
    box_texture* texture, 
    const void* data, 
    uvec2 offset, 
    uvec2 region);

void vulkan_texture_transition_layout(
	box_renderer_backend* backend,
    vulkan_command_buffer* cmd, 
    box_texture* texture,
    VkImageLayout new_layout);

void vulkan_texture_destroy(
	box_renderer_backend* backend,
    box_texture* texture);
