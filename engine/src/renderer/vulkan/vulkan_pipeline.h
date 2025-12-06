#pragma once

#include "defines.h"

#include "vulkan_types.h"
#include "platform/filesystem.h"

b8 vulkan_graphics_pipeline_create(
    vulkan_context* context,
    vulkan_graphics_pipeline* out_pipeline,
    vulkan_renderpass* renderpass,
    box_shader* shader);

void vulkan_graphics_pipeline_destroy(vulkan_context* context, vulkan_graphics_pipeline* pipeline);

void vulkan_graphics_pipeline_bind(vulkan_command_buffer* command_buffer, vulkan_graphics_pipeline* pipeline);