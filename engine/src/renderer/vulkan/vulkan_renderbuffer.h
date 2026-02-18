#pragma once

#include "defines.h"

#include "vulkan_types.h"

b8 vulkan_renderbuffer_create(
	box_renderer_backend* backend,
	box_renderbuffer* out_buffer);

b8 vulkan_renderbuffer_upload_data(
	box_renderer_backend* backend,
	box_renderbuffer* buffer,
	const void* buf_data,
    u64 buf_offset, 
    u64 buf_size);

void vulkan_renderbuffer_destroy(
	box_renderer_backend* backend,
	box_renderbuffer* buffer);