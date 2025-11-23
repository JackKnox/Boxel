#pragma once

#include "defines.h"

#include "renderer/renderer_backend.h"

b8 vulkan_renderer_backend_initialize(renderer_backend* backend, const char* application_name, renderer_backend_config* config);
void vulkan_renderer_backend_shutdown(renderer_backend* backend);

void vulkan_renderer_backend_on_resized(renderer_backend* backend, u32 width, u32 height);

b8 vulkan_renderer_backend_begin_frame(renderer_backend* backend, f32 delta_time);
b8 vulkan_renderer_playback_rendercmd(renderer_backend* backend, struct box_rendercmd* rendercmd);
b8 vulkan_renderer_backend_end_frame(renderer_backend* backend);