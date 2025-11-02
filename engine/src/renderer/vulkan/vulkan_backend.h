#pragma once

#include "defines.h"

#include "renderer/renderer_backend.h"

struct box_engine;

b8 vulkan_renderer_backend_initialize(renderer_backend* backend, renderer_backend_config* config);
void vulkan_renderer_backend_shutdown(renderer_backend* backend);

void vulkan_renderer_backend_on_resized(renderer_backend* backend, u16 width, u16 height);

b8 vulkan_renderer_backend_begin_frame(renderer_backend* backend, f32 delta_time);
b8 vulkan_renderer_backend_end_frame(renderer_backend* backend);