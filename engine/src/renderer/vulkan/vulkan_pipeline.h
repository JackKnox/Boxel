#pragma once

#include "defines.h"

#include "vulkan_types.h"

VkResult vulkan_graphics_pipeline_create(
    vulkan_context* context,
    vulkan_pipeline* out_pipeline,
    vulkan_renderpass* renderpass,
    box_renderstage* shader);

VkResult vulkan_compute_pipeline_create(
    vulkan_context* context,
    vulkan_pipeline* out_pipeline,
    vulkan_renderpass* renderpass,
    box_renderstage* shader);

void vulkan_pipeline_destroy(vulkan_context* context, vulkan_pipeline* pipeline);

void vulkan_pipeline_bind(vulkan_command_buffer* command_buffer, vulkan_pipeline* pipeline);