#include "defines.h"
#include "vulkan_device.h"

#include "utils/darray.h"
#include "utils/string_utils.h"

b8 select_physical_device(vulkan_context* context);
b8 physical_device_meets_requirements(
    VkPhysicalDevice device,
    const vulkan_context* context,
    renderer_capabilities* out_capabilities,
    vulkan_swapchain_support_info* out_swapchain_support);

VkResult vulkan_device_create(vulkan_context* context) {
    darray_push(context->config->required_extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    if (!select_physical_device(context)) {
        BX_ERROR("Could not find device which supported requested GPU features.");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // NOTE: Do not create additional queues for shared indices
    u32 graphics_index = context->config->capabilities.graphics_queue_index;
    u32 present_index = context->config->capabilities.present_queue_index;
    u32 transfer_index = context->config->capabilities.transfer_queue_index;

    // TODO: I don't this code, so fix it I guess.
    b8 present_shares_graphics = (graphics_index == present_index);
    b8 transfer_shares_graphics = (graphics_index == transfer_index);

    u32 indices[3];
    u32 index_count = 0;
    indices[index_count++] = graphics_index;

    if (!present_shares_graphics)
        indices[index_count++] = present_index;

    if (!transfer_shares_graphics && transfer_index != present_index)
        indices[index_count++] = transfer_index;

    VkDeviceQueueCreateInfo queue_create_infos[3];

    f32 queue_priority = 1.0f;
    for (u32 i = 0; i < index_count; ++i) {
        queue_create_infos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_infos[i].queueFamilyIndex = indices[i];
        queue_create_infos[i].queueCount = 1;
        queue_create_infos[i].flags = 0;
        queue_create_infos[i].pNext = 0;
        queue_create_infos[i].pQueuePriorities = &queue_priority;
    }

    // Request device features.
    VkPhysicalDeviceFeatures device_features = {0};
    device_features.samplerAnisotropy = context->config->sampler_anisotropy;  // Request anistrophy

    VkDeviceCreateInfo device_create_info = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    device_create_info.queueCreateInfoCount = index_count;
    device_create_info.pQueueCreateInfos = queue_create_infos;
    device_create_info.pEnabledFeatures = &device_features;
    device_create_info.enabledExtensionCount = darray_length(context->config->required_extensions);
    device_create_info.ppEnabledExtensionNames = context->config->required_extensions;

    // Create the device.
    VkResult result = vkCreateDevice(context->device.physical_device, &device_create_info, context->allocator, &context->device.logical_device);
    if (!vulkan_result_is_success(result)) return result;

    BX_INFO("Logical device created.");

    // Get queues.
    vkGetDeviceQueue(
        context->device.logical_device,
        context->config->capabilities.graphics_queue_index,
        0,
        &context->device.graphics_queue);

    vkGetDeviceQueue(
        context->device.logical_device,
        context->config->capabilities.present_queue_index,
        0,
        &context->device.present_queue);

    vkGetDeviceQueue(
        context->device.logical_device,
        context->config->capabilities.transfer_queue_index,
        0,
        &context->device.transfer_queue);

    BX_INFO("Queues obtained.");

    // Create command pool for graphics queue.
    VkCommandPoolCreateInfo pool_create_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    pool_create_info.queueFamilyIndex = context->config->capabilities.graphics_queue_index;
    pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    result = vkCreateCommandPool(context->device.logical_device, &pool_create_info, context->allocator, &context->device.graphics_command_pool);
    if (!vulkan_result_is_success(result)) return result;

    BX_TRACE("Graphics command pool created.");
    return TRUE;
}

void vulkan_device_destroy(vulkan_context* context) {
    if (!context->device.logical_device) return;

    // Unset queues
    context->device.graphics_queue = 0;
    context->device.present_queue = 0;
    context->device.transfer_queue = 0;

    BX_INFO("Destroying command pools...");
    vkDestroyCommandPool(
        context->device.logical_device,
        context->device.graphics_command_pool,
        context->allocator);

    // Destroy logical device
    BX_INFO("Destroying rendering device...");
    if (context->device.logical_device) {
        vkDestroyDevice(context->device.logical_device, context->allocator);
        context->device.logical_device = 0;
    }

    context->device.physical_device = 0;

    vulkan_swapchain_support_info* support_info = &context->device.swapchain_support;
    if (support_info->formats) {
        bfree(
            context->device.swapchain_support.formats,
            sizeof(VkSurfaceFormatKHR) * support_info->format_count, MEMORY_TAG_RENDERER);
    }

    if (support_info->present_modes) {
        bfree(support_info->present_modes,
            sizeof(VkPresentModeKHR) * support_info->present_mode_count, MEMORY_TAG_RENDERER);
    }

    platform_zero_memory(
        support_info,
        sizeof(vulkan_swapchain_support_info));
}

void vulkan_device_query_swapchain_support(
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface,
    vulkan_swapchain_support_info* out_support_info) {
    // Surface capabilities
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        physical_device,
        surface,
        &out_support_info->capabilities));

    // Surface formats
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        physical_device,
        surface,
        &out_support_info->format_count,
        0));

    if (out_support_info->format_count != 0) {
        if (!out_support_info->formats) {
            out_support_info->formats = ballocate(sizeof(VkSurfaceFormatKHR) * out_support_info->format_count, MEMORY_TAG_RENDERER);
        }
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
            physical_device,
            surface,
            &out_support_info->format_count,
            out_support_info->formats));
    }

    // Present modes
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
        physical_device,
        surface,
        &out_support_info->present_mode_count,
        0));
    if (out_support_info->present_mode_count != 0) {
        if (!out_support_info->present_modes) {
            out_support_info->present_modes = ballocate(sizeof(VkPresentModeKHR) * out_support_info->present_mode_count, MEMORY_TAG_RENDERER);
        }
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
            physical_device,
            surface,
            &out_support_info->present_mode_count,
            out_support_info->present_modes));
    }
}

b8 vulkan_device_detect_depth_format(vulkan_device* device) {
    // Format candidates
    const u64 candidate_count = 3;
    VkFormat candidates[3] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT};

    u32 flags = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    for (u64 i = 0; i < candidate_count; ++i) {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(device->physical_device, candidates[i], &properties);

        if ((properties.linearTilingFeatures & flags) == flags) {
            device->depth_format = candidates[i];
            return TRUE;
        } else if ((properties.optimalTilingFeatures & flags) == flags) {
            device->depth_format = candidates[i];
            return TRUE;
        }
    }

    return FALSE;
}

b8 select_physical_device(vulkan_context* context) {
    u32 physical_device_count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(context->instance, &physical_device_count, 0));
    if (physical_device_count == 0) {
        BX_ERROR("No devices which support Vulkan were found.");
        return FALSE;
    }

    VkPhysicalDevice* physical_devices = darray_reserve(VkPhysicalDevice, physical_device_count, MEMORY_TAG_RENDERER);
    VK_CHECK(vkEnumeratePhysicalDevices(context->instance, &physical_device_count, physical_devices));

    for (u32 i = 0; i < physical_device_count; ++i) {
        renderer_capabilities capabilities = {0};
        b8 result = physical_device_meets_requirements(
            physical_devices[i],
            context,
            &capabilities,
            &context->device.swapchain_support);

        if (result) {
            BX_INFO("Selected device: '%s'.", capabilities.device_name);

            // GPU type, etc.
            switch (capabilities.device_type) {
                default:
                case RENDERER_DEVICE_TYPE_OTHER:
                    BX_INFO("GPU type is Unknown.");
                    break;
                case RENDERER_DEVICE_TYPE_INTEGRATED_GPU:
                    BX_INFO("GPU type is Integrated.");
                    break;
                case RENDERER_DEVICE_TYPE_DISCRETE_GPU:
                    BX_INFO("GPU type is Descrete.");
                    break;
                case RENDERER_DEVICE_TYPE_VIRTUAL_GPU:
                    BX_INFO("GPU type is Virtual.");
                    break;
                case RENDERER_DEVICE_TYPE_CPU:
                    BX_INFO("GPU type is CPU.");
                    break;
            }

            context->device.physical_device = physical_devices[i];
            context->config->capabilities = capabilities;
            break;
        }
    }

    darray_destroy(physical_devices);

    // Ensure a device was selected
    if (!context->device.physical_device) {
        BX_ERROR("No physical devices were found which meet the requirements.");
        return FALSE;
    }

    BX_INFO("Physical device selected.");
    return TRUE;
}

b8 physical_device_meets_requirements(
    VkPhysicalDevice device,
    const vulkan_context* context,
    renderer_capabilities* out_capabilities,
    vulkan_swapchain_support_info* out_swapchain_support) {
    // Evaluate device properties to determine if it meets the needs of our applcation.
    out_capabilities->graphics_queue_index = -1;
    out_capabilities->present_queue_index = -1;
    out_capabilities->compute_queue_index = -1;
    out_capabilities->transfer_queue_index = -1;

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device, &properties);

    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(device, &features);

    VkPhysicalDeviceMemoryProperties memory;
    vkGetPhysicalDeviceMemoryProperties(device, &memory);

    out_capabilities->device_type = (renderer_device_type)properties.deviceType;
    strncpy(out_capabilities->device_name, properties.deviceName, sizeof(out_capabilities->device_name));

    // Discrete GPU?
    if (context->config->discrete_gpu) {
        if (properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            BX_INFO("Device is not a discrete GPU, and one is required. Skipping.");
            return FALSE;
        }
    }

    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, 0);
    VkQueueFamilyProperties* queue_families = darray_reserve(VkQueueFamilyProperties, queue_family_count, MEMORY_TAG_RENDERER);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families);

    // Look at each queue and see what queues it supports
    BX_INFO("Graphics | Present | Compute | Transfer | Name");
    u8 min_transfer_score = 255;
    for (u32 i = 0; i < queue_family_count; ++i) {
        u8 current_transfer_score = 0;

        // Graphics queue?
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            out_capabilities->graphics_queue_index = i;
            ++current_transfer_score;
        }

        // Compute queue?
        if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            out_capabilities->compute_queue_index = i;
            ++current_transfer_score;
        }

        // Transfer queue?
        if (queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
            // Take the index if it is the current lowest. This increases the
            // liklihood that it is a dedicated transfer queue.
            if (current_transfer_score <= min_transfer_score) {
                min_transfer_score = current_transfer_score;
                out_capabilities->transfer_queue_index = i;
            }
        }

        // Present queue?
        VkBool32 supports_present = VK_FALSE;
        VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, context->surface, &supports_present));
        if (supports_present) {
            out_capabilities->present_queue_index = i;
        }
    }

    darray_destroy(queue_families);

    // Print out some info about the device
    BX_INFO("       %d |       %d |       %d |        %d | %s",
          out_capabilities->graphics_queue_index != -1,
          out_capabilities->present_queue_index != -1,
          out_capabilities->compute_queue_index != -1,
          out_capabilities->transfer_queue_index != -1,
          properties.deviceName);

    b8 need_present_queue = (context->config->graphics_pipeline);

    if (
        (!context->config->graphics_pipeline || (context->config->graphics_pipeline && out_capabilities->graphics_queue_index != -1)) &&
        (!need_present_queue                 || (need_present_queue && out_capabilities->present_queue_index != -1)) &&
        (!context->config->compute_pipeline  || (context->config->compute_pipeline && out_capabilities->compute_queue_index != -1)) &&
        (!context->config->enable_transfer   || (context->config->enable_transfer && out_capabilities->transfer_queue_index != -1))) {
        BX_INFO("Device meets queue requirements.");
        BX_TRACE("Graphics Family Index: %i", out_capabilities->graphics_queue_index);
        BX_TRACE("Present Family Index:  %i", out_capabilities->present_queue_index);
        BX_TRACE("Transfer Family Index: %i", out_capabilities->transfer_queue_index);
        BX_TRACE("Compute Family Index:  %i", out_capabilities->compute_queue_index);

        // Query swapchain support.
        vulkan_device_query_swapchain_support(
            device,
            context->surface,
            out_swapchain_support);

        if (out_swapchain_support->format_count < 1 || out_swapchain_support->present_mode_count < 1) {
            if (out_swapchain_support->formats) {
                bfree(out_swapchain_support->formats, 
                    sizeof(VkSurfaceFormatKHR) * out_swapchain_support->format_count, 
                    MEMORY_TAG_RENDERER);
            }
            if (out_swapchain_support->present_modes) {
                bfree(out_swapchain_support->present_modes, 
                    sizeof(VkPresentModeKHR) * out_swapchain_support->present_mode_count,
                    MEMORY_TAG_RENDERER);
            }
            BX_INFO("Required swapchain support not present, skipping device.");
            return FALSE;
        }

        // Device extensions.
        if (darray_length(context->config->required_extensions) > 0) {
            u32 available_extension_count = 0;
            VkExtensionProperties* available_extensions = 0;
            VK_CHECK(vkEnumerateDeviceExtensionProperties(
                device,
                0,
                &available_extension_count,
                0));
            if (available_extension_count != 0) {
                available_extensions = ballocate(sizeof(VkExtensionProperties) * available_extension_count, MEMORY_TAG_RENDERER);
                VK_CHECK(vkEnumerateDeviceExtensionProperties(
                    device,
                    0,
                    &available_extension_count,
                    available_extensions));

                u32 required_extension_count = darray_length(context->config->required_extensions);
                for (u32 i = 0; i < required_extension_count; ++i) {
                    b8 found = FALSE;
                    for (u32 j = 0; j < available_extension_count; ++j) {
                        if (strings_equal(context->config->required_extensions[i], available_extensions[j].extensionName)) {
                            found = TRUE;
                            break;
                        }
                    }

                    if (!found) {
                        BX_INFO("Required extension not found: '%s', skipping device.", context->config->required_extensions[i]);
                        bfree(available_extensions, sizeof(VkExtensionProperties) * available_extension_count, MEMORY_TAG_RENDERER);
                        return FALSE;
                    }
                }
            }
            bfree(available_extensions, sizeof(VkExtensionProperties)* available_extension_count, MEMORY_TAG_RENDERER);
        }

        // Sampler anisotropy
        if (context->config->sampler_anisotropy && !features.samplerAnisotropy) {
            BX_INFO("Device does not support sampler anisotropy, skipping.");
            return FALSE;
        }

        // Device meets all requirements.
        return TRUE;
    }

    return FALSE;
}