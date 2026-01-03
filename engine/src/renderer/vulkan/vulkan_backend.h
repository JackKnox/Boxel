#pragma once

#include "defines.h"

#include "renderer/renderer_backend.h"
#include "renderer/renderer_types.h"
#include "renderer/renderer_cmd.h"

b8 vulkan_renderer_backend_initialize(box_renderer_backend* backend, box_renderer_backend_config* config, uvec2 starting_size, const char* application_name);
void vulkan_renderer_backend_shutdown(box_renderer_backend* backend);

void vulkan_renderer_backend_wait_until_idle(box_renderer_backend* backend, u64 timeout);
void vulkan_renderer_backend_on_resized(box_renderer_backend* backend, uvec2 new_size);

b8 vulkan_renderer_backend_begin_frame(box_renderer_backend* backend, f32 delta_time);
void vulkan_renderer_playback_rendercmd(box_renderer_backend* backend, box_rendercmd_context* rendercmd_context, rendercmd_header* header, rendercmd_payload* payload);
b8 vulkan_renderer_backend_end_frame(box_renderer_backend* backend);

b8 vulkan_renderer_create_renderstage(box_renderer_backend* backend, box_renderstage* out_stage);
void vulkan_renderer_destroy_renderstage(box_renderer_backend* backend, box_renderstage* stage);

b8 vulkan_renderer_create_renderbuffer(box_renderer_backend* backend, box_renderbuffer* out_buffer);
b8 vulkan_renderer_upload_to_renderbuffer(box_renderer_backend* backend, box_renderbuffer* buffer, void* data, u64 start_offset, u64 region);
void vulkan_renderer_destroy_renderbuffer(box_renderer_backend* backend, box_renderbuffer* buffer);

b8 vulkan_renderer_create_texture(box_renderer_backend* backend, box_texture* out_texture);
void vulkan_renderer_destroy_texture(box_renderer_backend* backend, box_texture* texture);