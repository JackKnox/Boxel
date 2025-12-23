#include "defines.h"
#include "vulkan_device.h"

#include "utils/darray.h"
#include "utils/string_utils.h"

renderer_mode queue_type_to_mode(vulkan_queue_type type) {
    switch (type) {
    case VULKAN_QUEUE_TYPE_GRAPHICS: return RENDERER_MODE_GRAPHICS;
    case VULKAN_QUEUE_TYPE_COMPUTE: return RENDERER_MODE_COMPUTE;
    case VULKAN_QUEUE_TYPE_TRANSFER: return RENDERER_MODE_TRANSFER;
    }

    return 0;
}

b8 select_physical_device(box_renderer_backend* backend);
b8 physical_device_meets_requirements(
    VkPhysicalDevice device,
    box_renderer_backend* backend,
    renderer_capabilities* out_capabilities,
    vulkan_queue out_queue_support[]);

VkResult vulkan_device_create(box_renderer_backend* backend) {
    vulkan_context* context = (vulkan_context*)backend->internal_context;

    if (!select_physical_device(backend)) {
        BX_ERROR("Could not find device which supported requested GPU features.");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    f32 queue_priority = 1.0f;
    VkDeviceQueueCreateInfo* queue_create_info = darray_create(VkDeviceQueueCreateInfo, MEMORY_TAG_RENDERER);

    for (u32 i = 0; i < VULKAN_QUEUE_TYPE_MAX; ++i) {
        u32 family = context->device.mode_queues[i].family_index;
        if (family == -1) continue;

        b8 exists = FALSE;
        for (u32 j = 0; j < darray_length(queue_create_info); ++j) {
            if (queue_create_info[j].queueFamilyIndex == family) {
                exists = TRUE;
                break;
            }
        }

        if (!exists) {
            VkDeviceQueueCreateInfo create_info = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
            create_info.queueFamilyIndex = family;
            create_info.queueCount = 1;
            create_info.pQueuePriorities = &queue_priority;
            queue_create_info = _darray_push(queue_create_info, &create_info);
        }
    }

    const char** required_extensions = darray_create(const char*, MEMORY_TAG_RENDERER);
    darray_push(required_extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    // Request device features.
    VkPhysicalDeviceFeatures device_features = {0};
    device_features.samplerAnisotropy = backend->config.sampler_anisotropy;  // Request anistrophy

    VkDeviceCreateInfo device_create_info = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    device_create_info.queueCreateInfoCount = darray_length(queue_create_info);
    device_create_info.pQueueCreateInfos = queue_create_info;
    device_create_info.pEnabledFeatures = &device_features;
    device_create_info.enabledExtensionCount = darray_length(required_extensions);
    device_create_info.ppEnabledExtensionNames = required_extensions;

    // Create the device.
    VkResult result = vkCreateDevice(context->device.physical_device, &device_create_info, context->allocator, &context->device.logical_device);
    if (!vulkan_result_is_success(result)) return result;

    BX_INFO("Logical device created.");

    darray_destroy(required_extensions);
    darray_destroy(queue_create_info);

    // Get queues.
    for (u32 i = 0; i < VULKAN_QUEUE_TYPE_MAX; ++i) {
        vulkan_queue* mode = &context->device.mode_queues[i];
        if (!(backend->config.modes & mode->supported_modes))
            continue;

        vkGetDeviceQueue(
            context->device.logical_device,
            mode->family_index,
            0,
            &mode->handle);
    }

    BX_INFO("Queues obtained.");

    // Create command pool for necessary queue.
    for (u32 i = 0; i < VULKAN_QUEUE_TYPE_MAX; ++i) {
        vulkan_queue* mode = &context->device.mode_queues[i];
        if (!(backend->config.modes & mode->supported_modes) || i == VULKAN_QUEUE_TYPE_PRESENT)
            continue;
        
        VkCommandPoolCreateInfo pool_create_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        pool_create_info.queueFamilyIndex = mode->family_index;
        pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VkResult result = vkCreateCommandPool(context->device.logical_device, &pool_create_info, context->allocator, &mode->pool);
        if (!vulkan_result_is_success(result)) return result;
    }

    BX_TRACE("Vulkan command pools created.");
    return VK_SUCCESS;
}

void vulkan_device_destroy(box_renderer_backend* backend) {
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    if (!context->device.logical_device) return;

    BX_INFO("Destroying command pools...");

    for (u32 i = 0; i < VULKAN_QUEUE_TYPE_MAX; ++i) {
        vulkan_queue* queue = &context->device.mode_queues[i];
        queue->handle = VK_NULL_HANDLE;
        queue->family_index = -1;
        queue->supported_modes = 0;
        
        if (queue->pool) {
            vkDestroyCommandPool(
                context->device.logical_device,
                queue->pool,
                context->allocator);
        }
    }

    // Destroy logical device
    BX_INFO("Destroying rendering device...");
    if (context->device.logical_device) {
        vkDestroyDevice(context->device.logical_device, context->allocator);
        context->device.logical_device = 0;
    }

    bfree(backend->config.capabilities.device_name, string_length(backend->config.capabilities.device_name) + 1, MEMORY_TAG_RENDERER);
    context->device.physical_device = 0;

    darray_destroy(context->device.swapchain_support.formats);
    darray_destroy(context->device.swapchain_support.present_modes);
}

void vulkan_device_query_swapchain_support(
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface,
    vulkan_swapchain_support_info* out_support_info) {
    // Surface capabilities
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &out_support_info->capabilities));

    // Surface formats
    u32 format_count = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, 0));

    if (format_count != 0) {
        if (!out_support_info->formats)
            out_support_info->formats = darray_reserve(VkSurfaceFormatKHR, format_count, MEMORY_TAG_RENDERER);

        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
            physical_device,
            surface,
            &format_count,
            out_support_info->formats));

        darray_length_set(out_support_info->formats, format_count);
    }

    // Present modes
    u32 present_mode_count = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, 0));

    if (present_mode_count != 0) {
        if (!out_support_info->present_modes)
            out_support_info->present_modes = darray_reserve(VkPresentModeKHR, present_mode_count, MEMORY_TAG_RENDERER);

        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
            physical_device,
            surface,
            &present_mode_count,
            out_support_info->present_modes));

        darray_length_set(out_support_info->present_modes, present_mode_count);
    }
}

b8 select_physical_device(box_renderer_backend* backend) {
    vulkan_context* context = (vulkan_context*)backend->internal_context;

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
            backend,
            &capabilities,
            context->device.mode_queues);

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
            backend->config.capabilities = capabilities;
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
    box_renderer_backend* backend,
    renderer_capabilities* out_capabilities,
    vulkan_queue out_queue_support[]) {
    // Evaluate device properties to determine if it meets the needs of our applcation.
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    vulkan_swapchain_support_info* support = &context->device.swapchain_support;

    darray_destroy(support->formats);
    darray_destroy(support->present_modes);
    support->formats = 0;
    support->present_modes = 0;

    for (u32 i = 0; i < VULKAN_QUEUE_TYPE_MAX; ++i) {
        out_queue_support[i].family_index = -1;
        out_queue_support[i].supported_modes = 0;
    }

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device, &properties);

    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(device, &features);

    VkPhysicalDeviceMemoryProperties memory;
    vkGetPhysicalDeviceMemoryProperties(device, &memory);

    out_capabilities->device_type = (renderer_device_type)properties.deviceType;

    if (out_capabilities->device_name)
        bfree(out_capabilities->device_name, string_length(out_capabilities->device_name) + 1, MEMORY_TAG_RENDERER);

    u64 str_length = string_length(properties.deviceName) + 1;
    out_capabilities->device_name = ballocate(str_length, MEMORY_TAG_RENDERER);
    strncpy(out_capabilities->device_name, properties.deviceName, str_length);

    // Discrete GPU?
    if (backend->config.discrete_gpu) {
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
            out_queue_support[VULKAN_QUEUE_TYPE_GRAPHICS].family_index = i;
            out_queue_support[VULKAN_QUEUE_TYPE_GRAPHICS].supported_modes |= RENDERER_MODE_GRAPHICS;
            ++current_transfer_score;
        }

        // Compute queue?
        if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            out_queue_support[VULKAN_QUEUE_TYPE_COMPUTE].family_index = i;
            out_queue_support[VULKAN_QUEUE_TYPE_COMPUTE].supported_modes |= RENDERER_MODE_COMPUTE;
            ++current_transfer_score;
        }

        // Transfer queue?
        if (queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
            // Take the index if it is the current lowest. This increases the
            // liklihood that it is a dedicated transfer queue.
            if (current_transfer_score <= min_transfer_score) {
                min_transfer_score = current_transfer_score;
                out_queue_support[VULKAN_QUEUE_TYPE_TRANSFER].family_index = i;
                out_queue_support[VULKAN_QUEUE_TYPE_TRANSFER].supported_modes |= RENDERER_MODE_TRANSFER;
            }
        }

        // Present queue?
        VkBool32 supports_present = VK_FALSE;
        VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, context->surface, &supports_present));
        if (supports_present) {
            out_queue_support[VULKAN_QUEUE_TYPE_PRESENT].family_index = i;
            out_queue_support[VULKAN_QUEUE_TYPE_PRESENT].supported_modes |= RENDERER_MODE_GRAPHICS;
        }
    }

    darray_destroy(queue_families);

    // Print out some info about the device
    BX_INFO("       %d |       %d |       %d |        %d | %s",
        out_queue_support[VULKAN_QUEUE_TYPE_GRAPHICS].family_index != -1,
        out_queue_support[VULKAN_QUEUE_TYPE_PRESENT].family_index != -1,
        out_queue_support[VULKAN_QUEUE_TYPE_COMPUTE].family_index != -1,
        out_queue_support[VULKAN_QUEUE_TYPE_TRANSFER].family_index != -1,
        properties.deviceName);

    b8 needs_present = backend->config.modes & RENDERER_MODE_GRAPHICS;

    if (
        (!(backend->config.modes & RENDERER_MODE_GRAPHICS) || ((backend->config.modes & RENDERER_MODE_GRAPHICS) && out_queue_support[VULKAN_QUEUE_TYPE_GRAPHICS].family_index != -1)) &&
        (!needs_present                                    || (needs_present                                    && out_queue_support[VULKAN_QUEUE_TYPE_PRESENT].family_index != -1)) &&
        (!(backend->config.modes & RENDERER_MODE_COMPUTE)  || ((backend->config.modes & RENDERER_MODE_COMPUTE)  && out_queue_support[VULKAN_QUEUE_TYPE_COMPUTE].family_index != -1)) &&
        (!(backend->config.modes & RENDERER_MODE_TRANSFER) || ((backend->config.modes & RENDERER_MODE_TRANSFER) && out_queue_support[VULKAN_QUEUE_TYPE_TRANSFER].family_index != -1))) {
        BX_INFO("Device meets queue requirements.");
        BX_TRACE("Graphics Family Index: %i", out_queue_support[VULKAN_QUEUE_TYPE_GRAPHICS].family_index);
        BX_TRACE("Present Family Index:  %i", out_queue_support[VULKAN_QUEUE_TYPE_PRESENT].family_index);
        BX_TRACE("Transfer Family Index: %i", out_queue_support[VULKAN_QUEUE_TYPE_TRANSFER].family_index);
        BX_TRACE("Compute Family Index:  %i", out_queue_support[VULKAN_QUEUE_TYPE_COMPUTE].family_index);

        // Query swapchain support.
        vulkan_device_query_swapchain_support(
            device,
            context->surface,
            &context->device.swapchain_support);

        if (darray_length(context->device.swapchain_support.formats) < 1 || darray_length(context->device.swapchain_support.present_modes) < 1) {
            BX_INFO("Required swapchain support not present, skipping device.");
            return FALSE;
        }

        // Sampler anisotropy
        if (backend->config.sampler_anisotropy && !features.samplerAnisotropy) {
            BX_INFO("Device does not support sampler anisotropy, skipping.");
            return FALSE;
        }

        // Device meets all requirements.
        return TRUE;
    }

    return FALSE;
}