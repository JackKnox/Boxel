#pragma once

#include "defines.h"

#include "vulkan_types.h"

typedef struct vulkan_window_system {
	VkSurfaceKHR surface;
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR* formats;
    VkPresentModeKHR* present_modes;

    box_rendertarget surface_rendertarget;

    VkSwapchainKHR swapchain;
    VkSurfaceFormatKHR swapchain_format;
    vulkan_image* swapchain_images;
    u32 swapchain_image_count;
} vulkan_window_system;

b8 vulkan_window_system_create(
    vulkan_context* context, 
    box_platform* plat_state,
    uvec2 window_size,
    vulkan_window_system* out_window_system);

void vulkan_window_system_destroy(vulkan_context* context, vulkan_window_system* window_system);

b8 vulkan_window_system_connect_rendertarget(vulkan_context* context, vulkan_window_system* window_system, box_rendertarget** out_rendertarget);

//void vulkan_window_system_on_resized(vulkan_context* context, vulkan_window_system* window_system, uvec2 new_size);

b8 vulkan_window_system_acquire_frame(vulkan_context* context, vulkan_window_system* window_system, f64 delta_time, u64 timeout);

b8 vulkan_window_system_present(vulkan_context* context, vulkan_window_system* window_system, VkQueue queue, VkSemaphore* wait_semaphores, u32 wait_semaphore_count);