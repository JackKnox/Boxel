#pragma once

#include "defines.h"

#include "renderer/renderer_backend.h"

b8 vulkan_renderer_backend_initialize(box_renderer_backend* backend, box_renderer_backend_config* config);
void vulkan_renderer_backend_shutdown(box_renderer_backend* backend);

void vulkan_renderer_backend_on_resized(box_renderer_backend* backend, uvec2 new_size);

b8 vulkan_renderer_backend_begin_frame(box_renderer_backend* backend, f64 delta_time);
void vulkan_renderer_execute_command(box_renderer_backend* backend, box_rendercmd_context* rendercmd_context, rendercmd_header* header, rendercmd_payload* payload);
b8 vulkan_renderer_backend_end_frame(box_renderer_backend* backend);