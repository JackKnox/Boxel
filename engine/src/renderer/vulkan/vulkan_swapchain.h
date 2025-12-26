#pragma once

#include "defines.h"

#include "vulkan_types.h"

VkResult vulkan_swapchain_create(
    box_renderer_backend* backend,
    vec2 size,
    vulkan_swapchain* out_swapchain);

VkResult vulkan_swapchain_recreate(
    box_renderer_backend* backend,
    vec2 size,
    vulkan_swapchain* swapchain);

void vulkan_swapchain_destroy(
    box_renderer_backend* backend,
    vulkan_swapchain* swapchain);

VkResult vulkan_swapchain_acquire_next_image_index(
    vulkan_context* context,
    vulkan_swapchain* swapchain,
    u64 timeout_ns,
    VkSemaphore image_available_semaphore,
    VkFence fence,
    u32* out_image_index);

VkResult vulkan_swapchain_present(
    vulkan_context* context,
    vulkan_swapchain* swapchain,
    VkQueue present_queue,
    VkSemaphore render_complete_semaphore,
    u32 present_image_index);