#pragma once

#include "defines.h"

#include "vulkan_types.h"

VkResult vulkan_graphics_pipeline_create(
	vulkan_context* context, 
	box_renderstage* renderstage,
	vulkan_renderpass* renderpass,
	vulkan_pipeline* out_pipeline);

VkResult vulkan_compute_pipeline_create(
	vulkan_context* context, 
	box_renderstage* renderstage,
	vulkan_renderpass* renderpass,
	vulkan_pipeline* out_pipeline);

void vulkan_pipeline_destroy(vulkan_context* context, vulkan_pipeline* pipeline);

void vulkan_pipeline_bind(vulkan_command_buffer* command_buffer, vulkan_context* context, vulkan_pipeline* pipeline);