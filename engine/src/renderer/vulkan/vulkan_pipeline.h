#pragma once

#include "defines.h"

#include "vulkan_types.inl"
#include "platform/filesystem.h"

b8 vulkan_graphics_pipeline_create(
    vulkan_context* context,
    vulkan_graphics_pipeline* out_pipeline,
    vulkan_renderpass* renderpass,
    const char** shader_paths,
    u32 shader_count);

void vulkan_graphics_pipeline_destroy(vulkan_context* context, vulkan_graphics_pipeline* pipeline);

void vulkan_graphics_pipeline_bind(vulkan_command_buffer* command_buffer, vulkan_graphics_pipeline* pipeline);

b8 vulkan_compute_pipeline_create(
    vulkan_context* context,
    vulkan_compute_pipeline* out_pipeline,
    const char* shader_path);

void vulkan_compute_pipeline_destroy(vulkan_context* context, vulkan_compute_pipeline* pipeline);