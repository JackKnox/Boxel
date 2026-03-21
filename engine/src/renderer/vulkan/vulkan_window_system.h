#pragma once

#include "defines.h"

#include "vulkan_types.h"

VkResult vulkan_window_system_create(
    box_renderer_backend* backend,
    vulkan_window_system* out_window_system);

void vulkan_window_system_destroy(
    box_renderer_backend* backend, 
    vulkan_window_system* window_system);

//void vulkan_window_system_on_resized(box_renderer_backend* backend, vulkan_window_system* window_system, uvec2 new_size);

VkResult vulkan_window_system_acquire(box_renderer_backend* backend, vulkan_window_system* window_system, u64 timeout, VkFence wait_fence);

VkResult vulkan_window_system_present(box_renderer_backend* backend, vulkan_window_system* window_system, vulkan_queue* present_queue, u32 wait_semaphore_count, VkSemaphore* wait_semaphores);