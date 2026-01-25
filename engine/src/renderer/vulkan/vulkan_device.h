#pragma once

#include "defines.h"

#include "vulkan_types.h"

VkResult vulkan_device_create(box_renderer_backend* backend);

void vulkan_device_destroy(box_renderer_backend* backend);

void vulkan_device_query_swapchain_support(
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface,
    vulkan_swapchain_support_info* out_support_info);