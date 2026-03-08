#pragma once

#include "defines.h"

#include "platform/platform.h"

#include <vulkan/vulkan.h>

/**
 * @brief Creates and assigns a surface to the given platform.
 *
 * @param instance Vulkan instance.
 * @param platform Connected platform to assign surface to.
 * @param allocator Global Vulkan allocator.
 * @param out_surface Ouputted surface handle.
 * 
 * @return Vulkan return code (VkResult)
 */
VkResult vulkan_platform_create_surface(VkInstance instance, box_platform* platform, const VkAllocationCallbacks* allocator, VkSurfaceKHR* out_surface);

/**
 * @brief Appends the names of required extensions for this platform to the @ref out_array.
 * 
 * @param out_array A pointer to the array names of required extension names.
 * 
 * @return Number of outputted extension names.
 */
u32 vulkan_platform_get_required_extensions(const char*** out_array);

/**
 * @brief Indicates if the given device/queue family index combo supports presentation.
 */
b8 vulkan_platform_presentation_support(VkInstance instance, VkPhysicalDevice physical_device, u32 queue_family_index);