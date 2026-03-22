#pragma once

#include "defines.h"

#include "vulkan_types.h"

// Creates and initializes a platform surface / swapchain and synchronization primitives.
VkResult vulkan_window_system_create(
    box_renderer_backend* backend,
    vulkan_window_system* out_window_system);

// Destroys a Vulkan window system and associated resources.
void vulkan_window_system_destroy(
    box_renderer_backend* backend, 
    vulkan_window_system* window_system);

//void vulkan_window_system_on_resized(box_renderer_backend* backend, vulkan_window_system* window_system, uvec2 new_size);

// Acquires the next available swapchain image for rendering.
VkResult vulkan_window_system_acquire(box_renderer_backend* backend, vulkan_window_system* window_system, u64 timeout, VkFence wait_fence);

// Presents the current swapchain image to the screen.
VkResult vulkan_window_system_present(box_renderer_backend* backend, vulkan_window_system* window_system, vulkan_queue* present_queue, u32 wait_semaphore_count, VkSemaphore* wait_semaphores);