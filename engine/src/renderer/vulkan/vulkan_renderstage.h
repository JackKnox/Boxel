#pragma once

#include "defines.h"

#include "vulkan_types.h"

b8 vulkan_renderstage_create(
	box_renderer_backend* backend,
	box_rendertarget* bound_rendertarget,
	box_renderstage* out_renderstage);

b8 vulkan_renderstage_update_descriptors(
    box_renderer_backend* backend,
    box_update_descriptors* descriptors, 
    u32 descriptor_count);

void vulkan_renderstage_bind(
	box_renderer_backend* backend,
    vulkan_command_buffer* command_buffer,
	box_renderstage* renderstage);

void vulkan_renderstage_destroy(
	box_renderer_backend* backend,
	box_renderstage* renderstage);
