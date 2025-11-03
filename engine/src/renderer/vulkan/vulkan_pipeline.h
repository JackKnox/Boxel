#pragma once

#include "defines.h"

#include "vulkan_types.inl"
#include "platform/filesystem.h"

b8 vulkan_graphics_pipeline_create(
    vulkan_context* context,
    vulkan_graphics_pipeline* out_pipeline,
    vulkan_renderpass* renderpass,
    VkShaderModule vert_shader, 
    VkShaderModule frag_shader,
    b8 is_wireframe,
    VkViewport viewport,
    VkRect2D scissor);


void vulkan_graphics_pipeline_destroy(vulkan_context* context, vulkan_graphics_pipeline* pipeline);

void vulkan_graphics_pipeline_bind(vulkan_command_buffer* command_buffer, VkPipelineBindPoint bind_point, vulkan_graphics_pipeline* pipeline);