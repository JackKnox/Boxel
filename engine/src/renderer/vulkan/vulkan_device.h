#pragma once

#include "defines.h"
#include "vulkan_types.h"

/**
 * @brief Creates and initializes the Vulkan logical device and selects a suitable physical device.
 *
 * This includes selecting queue families, enabling required device features,
 * and creating the logical device and associated queues.
 *
 * @param backend Pointer to the renderer backend containing Vulkan context data.
 *
 * @return VkResult indicating success or failure of device creation.
 */
VkResult vulkan_device_create(
    box_renderer_backend* backend);

/**
 * @brief Destroys the Vulkan logical device and releases associated resources.
 *
 * Cleans up queues and any device-level allocations created during
 * vulkan_device_create().
 *
 * @param backend Pointer to the renderer backend containing Vulkan context data.
 */
void vulkan_device_destroy(
    box_renderer_backend* backend);