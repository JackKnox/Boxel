#pragma once

#include "defines.h"
#include "vulkan_types.h"

// Creates and initializes the Vulkan logical device and selects a suitable physical device.
VkResult vulkan_device_create(
    box_renderer_backend* backend);

// Destroys the Vulkan logical device and releases associated resources.
void vulkan_device_destroy(
    box_renderer_backend* backend);