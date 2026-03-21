#include "defines.h"
#include "vulkan_types.h"

i32 find_memory_index(vulkan_context *context, u32 type_filter, u32 property_flags) {
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(context->device.physical_device, &memory_properties);

    for (u32 i = 0; i < memory_properties.memoryTypeCount; ++i) {
        // Check each memory type to see if its bit is set to 1.
        if (type_filter & (1 << i) && (memory_properties.memoryTypes[i].propertyFlags & property_flags) == property_flags)
            return i;
    }

    BX_WARN("Unable to find suitable memory type!");
    return -1;
}

VkShaderStageFlags box_shader_type_to_vulkan_type(box_shader_stage_type type) {
    switch (type) {
    case BOX_SHADER_STAGE_TYPE_VERTEX:  return VK_SHADER_STAGE_VERTEX_BIT;
    case BOX_SHADER_STAGE_TYPE_FRAGMENT: return VK_SHADER_STAGE_FRAGMENT_BIT;
    case BOX_SHADER_STAGE_TYPE_COMPUTE:  return VK_SHADER_STAGE_COMPUTE_BIT;

    default:
        BX_ASSERT(FALSE && "Unsupported shader stage type!");
        return 0;
    }
}

VkDescriptorType box_descriptor_type_to_vulkan_type(box_descriptor_type descriptor_type) {
    switch (descriptor_type) {
    case BOX_DESCRIPTOR_TYPE_STORAGE_BUFFER: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case BOX_DESCRIPTOR_TYPE_STORAGE_IMAGE:  return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case BOX_DESCRIPTOR_TYPE_IMAGE_SAMPLER:  return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    
    default:
        BX_ASSERT(FALSE && "Unsupported descriptor type!");
        return 0;
    }
}

VkFilter box_filter_to_vulkan_type(box_filter_type filter_type) {
    switch (filter_type) {
    case BOX_FILTER_TYPE_NEAREST: return VK_FILTER_NEAREST;
    case BOX_FILTER_TYPE_LINEAR:  return VK_FILTER_LINEAR;

    default:
        BX_ASSERT(FALSE && "Unsupported filter type!");
        return 0;
    }
}

VkSamplerAddressMode box_address_mode_to_vulkan_type(box_address_mode address) {
    switch (address) {
    case BOX_ADDRESS_MODE_REPEAT:               return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case BOX_ADDRESS_MODE_MIRRORED_REPEAT:      return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case BOX_ADDRESS_MODE_CLAMP_TO_EDGE:        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case BOX_ADDRESS_MODE_CLAMP_TO_BORDER:      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    case BOX_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    
    default:
        BX_ASSERT(FALSE && "Unsupported address mode!");
        return 0;
    }
}

VkFormat box_render_format_to_vulkan_type(box_render_format format) {
    switch (format) {
        /* 8-bit integer  */
        case BOX_FORMAT_R8_UINT:    return VK_FORMAT_R8_UINT;
        case BOX_FORMAT_R8_SINT:    return VK_FORMAT_R8_SINT;
        case BOX_FORMAT_RG8_UINT:   return VK_FORMAT_R8G8_UINT;
        case BOX_FORMAT_RG8_SINT:   return VK_FORMAT_R8G8_SINT;
        case BOX_FORMAT_RGB8_UINT:  return VK_FORMAT_R8G8B8_UINT;
        case BOX_FORMAT_RGB8_SINT:  return VK_FORMAT_R8G8B8_SINT;
        case BOX_FORMAT_RGBA8_UINT: return VK_FORMAT_R8G8B8A8_UINT;
        case BOX_FORMAT_RGBA8_SINT: return VK_FORMAT_R8G8B8A8_SINT;

        /* 8-bit normalized */
        case BOX_FORMAT_R8_UNORM:    return VK_FORMAT_R8_UNORM;
        case BOX_FORMAT_R8_SNORM:    return VK_FORMAT_R8_SNORM;
        case BOX_FORMAT_RG8_UNORM:   return VK_FORMAT_R8G8_UNORM;
        case BOX_FORMAT_RG8_SNORM:   return VK_FORMAT_R8G8_SNORM;
        case BOX_FORMAT_RGB8_UNORM:  return VK_FORMAT_R8G8B8_UNORM;
        case BOX_FORMAT_RGB8_SNORM:  return VK_FORMAT_R8G8B8_SNORM;
        case BOX_FORMAT_RGBA8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
        case BOX_FORMAT_RGBA8_SNORM: return VK_FORMAT_R8G8B8A8_SNORM;

        /* sRGB */
        case BOX_FORMAT_R8_SRGB:    return VK_FORMAT_R8_SRGB;
        case BOX_FORMAT_RG8_SRGB:   return VK_FORMAT_R8G8_SRGB;
        case BOX_FORMAT_RGB8_SRGB:  return VK_FORMAT_R8G8B8_SRGB;
        case BOX_FORMAT_RGBA8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;

        /* 16-bit */
        case BOX_FORMAT_R16_UINT:    return VK_FORMAT_R16_UINT;
        case BOX_FORMAT_R16_SINT:    return VK_FORMAT_R16_SINT;
        case BOX_FORMAT_R16_FLOAT:   return VK_FORMAT_R16_SFLOAT;

        case BOX_FORMAT_RG16_UINT:   return VK_FORMAT_R16G16_UINT;
        case BOX_FORMAT_RG16_SINT:   return VK_FORMAT_R16G16_SINT;
        case BOX_FORMAT_RG16_FLOAT:  return VK_FORMAT_R16G16_SFLOAT;

        case BOX_FORMAT_RGB16_UINT:  return VK_FORMAT_R16G16B16_UINT;
        case BOX_FORMAT_RGB16_SINT:  return VK_FORMAT_R16G16B16_SINT;
        case BOX_FORMAT_RGB16_FLOAT: return VK_FORMAT_R16G16B16_SFLOAT;

        case BOX_FORMAT_RGBA16_UINT:  return VK_FORMAT_R16G16B16A16_UINT;
        case BOX_FORMAT_RGBA16_SINT:  return VK_FORMAT_R16G16B16A16_SINT;
        case BOX_FORMAT_RGBA16_FLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;

        /* 32-bit */
        case BOX_FORMAT_R32_UINT:    return VK_FORMAT_R32_UINT;
        case BOX_FORMAT_R32_SINT:    return VK_FORMAT_R32_SINT;
        case BOX_FORMAT_R32_FLOAT:   return VK_FORMAT_R32_SFLOAT;

        case BOX_FORMAT_RG32_UINT:   return VK_FORMAT_R32G32_UINT;
        case BOX_FORMAT_RG32_SINT:   return VK_FORMAT_R32G32_SINT;
        case BOX_FORMAT_RG32_FLOAT:  return VK_FORMAT_R32G32_SFLOAT;

        case BOX_FORMAT_RGB32_UINT:  return VK_FORMAT_R32G32B32_UINT;
        case BOX_FORMAT_RGB32_SINT:  return VK_FORMAT_R32G32B32_SINT;
        case BOX_FORMAT_RGB32_FLOAT: return VK_FORMAT_R32G32B32_SFLOAT;

        case BOX_FORMAT_RGBA32_UINT:  return VK_FORMAT_R32G32B32A32_UINT;
        case BOX_FORMAT_RGBA32_SINT:  return VK_FORMAT_R32G32B32A32_SINT;
        case BOX_FORMAT_RGBA32_FLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;

        default:
            BX_ASSERT(FALSE && "Unsupported render format!");
            return VK_FORMAT_UNDEFINED;
    }
}

VkAttachmentLoadOp box_load_op_to_vulkan_type(box_load_op load_op) {
    switch (load_op) {
    case BOX_LOAD_OP_LOAD: return VK_ATTACHMENT_LOAD_OP_LOAD;
    case BOX_LOAD_OP_CLEAR: return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case BOX_LOAD_OP_DONT_CARE: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;

    default:
        BX_ASSERT(FALSE && "Unsupported load op!");
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
}

VkAttachmentStoreOp box_store_op_to_vulkan_type(box_store_op store_op) {
    switch (store_op) {
    case BOX_STORE_OP_STORE: return VK_ATTACHMENT_STORE_OP_STORE;
    case BOX_STORE_OP_DONT_CARE: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    
    default:
        BX_ASSERT(FALSE && "Unsupported store op!");
        return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }
}

const char* vulkan_result_string(VkResult result, b8 get_extended) {
    // From: https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkResult.html
    // Success Codes
    switch (result) {
    default:
    case VK_SUCCESS:
        return !get_extended ? "VK_SUCCESS" : "VK_SUCCESS Command successfully completed";
    case VK_NOT_READY:
        return !get_extended ? "VK_NOT_READY" : "VK_NOT_READY A fence or query has not yet completed";
    case VK_TIMEOUT:
        return !get_extended ? "VK_TIMEOUT" : "VK_TIMEOUT A wait operation has not completed in the specified time";
    case VK_EVENT_SET:
        return !get_extended ? "VK_EVENT_SET" : "VK_EVENT_SET An event is signaled";
    case VK_EVENT_RESET:
        return !get_extended ? "VK_EVENT_RESET" : "VK_EVENT_RESET An event is unsignaled";
    case VK_INCOMPLETE:
        return !get_extended ? "VK_INCOMPLETE" : "VK_INCOMPLETE A return array was too small for the result";
    case VK_SUBOPTIMAL_KHR:
        return !get_extended ? "VK_SUBOPTIMAL_KHR" : "VK_SUBOPTIMAL_KHR A swapchain no longer matches the surface properties exactly, but can still be used to present to the surface successfully.";
    case VK_THREAD_IDLE_KHR:
        return !get_extended ? "VK_THREAD_IDLE_KHR" : "VK_THREAD_IDLE_KHR A deferred operation is not complete but there is currently no work for this thread to do at the time of this call.";
    case VK_THREAD_DONE_KHR:
        return !get_extended ? "VK_THREAD_DONE_KHR" : "VK_THREAD_DONE_KHR A deferred operation is not complete but there is no work remaining to assign to additional threads.";
    case VK_OPERATION_DEFERRED_KHR:
        return !get_extended ? "VK_OPERATION_DEFERRED_KHR" : "VK_OPERATION_DEFERRED_KHR A deferred operation was requested and at least some of the work was deferred.";
    case VK_OPERATION_NOT_DEFERRED_KHR:
        return !get_extended ? "VK_OPERATION_NOT_DEFERRED_KHR" : "VK_OPERATION_NOT_DEFERRED_KHR A deferred operation was requested and no operations were deferred.";
    case VK_PIPELINE_COMPILE_REQUIRED_EXT:
        return !get_extended ? "VK_PIPELINE_COMPILE_REQUIRED_EXT" : "VK_PIPELINE_COMPILE_REQUIRED_EXT A requested pipeline creation would have required compilation, but the application requested compilation to not be performed.";

        // Error codes
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return !get_extended ? "VK_ERROR_OUT_OF_HOST_MEMORY" : "VK_ERROR_OUT_OF_HOST_MEMORY A host memory allocation has failed.";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return !get_extended ? "VK_ERROR_OUT_OF_DEVICE_MEMORY" : "VK_ERROR_OUT_OF_DEVICE_MEMORY A device memory allocation has failed.";
    case VK_ERROR_INITIALIZATION_FAILED:
        return !get_extended ? "VK_ERROR_INITIALIZATION_FAILED" : "VK_ERROR_INITIALIZATION_FAILED Initialization of an object could not be completed for implementation-specific reasons.";
    case VK_ERROR_DEVICE_LOST:
        return !get_extended ? "VK_ERROR_DEVICE_LOST" : "VK_ERROR_DEVICE_LOST The logical or physical device has been lost. See Lost Device";
    case VK_ERROR_MEMORY_MAP_FAILED:
        return !get_extended ? "VK_ERROR_MEMORY_MAP_FAILED" : "VK_ERROR_MEMORY_MAP_FAILED Mapping of a memory object has failed.";
    case VK_ERROR_LAYER_NOT_PRESENT:
        return !get_extended ? "VK_ERROR_LAYER_NOT_PRESENT" : "VK_ERROR_LAYER_NOT_PRESENT A requested layer is not present or could not be loaded.";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        return !get_extended ? "VK_ERROR_EXTENSION_NOT_PRESENT" : "VK_ERROR_EXTENSION_NOT_PRESENT A requested extension is not supported.";
    case VK_ERROR_FEATURE_NOT_PRESENT:
        return !get_extended ? "VK_ERROR_FEATURE_NOT_PRESENT" : "VK_ERROR_FEATURE_NOT_PRESENT A requested feature is not supported.";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        return !get_extended ? "VK_ERROR_INCOMPATIBLE_DRIVER" : "VK_ERROR_INCOMPATIBLE_DRIVER The requested version of Vulkan is not supported by the driver or is otherwise incompatible for implementation-specific reasons.";
    case VK_ERROR_TOO_MANY_OBJECTS:
        return !get_extended ? "VK_ERROR_TOO_MANY_OBJECTS" : "VK_ERROR_TOO_MANY_OBJECTS Too many objects of the type have already been created.";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
        return !get_extended ? "VK_ERROR_FORMAT_NOT_SUPPORTED" : "VK_ERROR_FORMAT_NOT_SUPPORTED A requested format is not supported on this device.";
    case VK_ERROR_FRAGMENTED_POOL:
        return !get_extended ? "VK_ERROR_FRAGMENTED_POOL" : "VK_ERROR_FRAGMENTED_POOL A pool allocation has failed due to fragmentation of the pool�s memory. This must only be returned if no attempt to allocate host or device memory was made to accommodate the new allocation. This should be returned in preference to VK_ERROR_OUT_OF_POOL_MEMORY, but only if the implementation is certain that the pool allocation failure was due to fragmentation.";
    case VK_ERROR_SURFACE_LOST_KHR:
        return !get_extended ? "VK_ERROR_SURFACE_LOST_KHR" : "VK_ERROR_SURFACE_LOST_KHR A surface is no longer available.";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
        return !get_extended ? "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR" : "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR The requested window is already in use by Vulkan or another API in a manner which prevents it from being used again.";
    case VK_ERROR_OUT_OF_DATE_KHR:
        return !get_extended ? "VK_ERROR_OUT_OF_DATE_KHR" : "VK_ERROR_OUT_OF_DATE_KHR A surface has changed in such a way that it is no longer compatible with the swapchain, and further presentation requests using the swapchain will fail. Applications must query the new surface properties and recreate their swapchain if they wish to continue presenting to the surface.";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
        return !get_extended ? "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR" : "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR The display used by a swapchain does not use the same presentable image layout, or is incompatible in a way that prevents sharing an image.";
    case VK_ERROR_INVALID_SHADER_NV:
        return !get_extended ? "VK_ERROR_INVALID_SHADER_NV" : "VK_ERROR_INVALID_SHADER_NV One or more shaders failed to compile or link. More details are reported back to the application via VK_EXT_debug_report if enabled.";
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        return !get_extended ? "VK_ERROR_OUT_OF_POOL_MEMORY" : "VK_ERROR_OUT_OF_POOL_MEMORY A pool memory allocation has failed. This must only be returned if no attempt to allocate host or device memory was made to accommodate the new allocation. If the failure was definitely due to fragmentation of the pool, VK_ERROR_FRAGMENTED_POOL should be returned instead.";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE:
        return !get_extended ? "VK_ERROR_INVALID_EXTERNAL_HANDLE" : "VK_ERROR_INVALID_EXTERNAL_HANDLE An external handle is not a valid handle of the specified type.";
    case VK_ERROR_FRAGMENTATION:
        return !get_extended ? "VK_ERROR_FRAGMENTATION" : "VK_ERROR_FRAGMENTATION A descriptor pool creation has failed due to fragmentation.";
    case VK_ERROR_INVALID_DEVICE_ADDRESS_EXT:
        return !get_extended ? "VK_ERROR_INVALID_DEVICE_ADDRESS_EXT" : "VK_ERROR_INVALID_DEVICE_ADDRESS_EXT A buffer creation failed because the requested address is not available.";
        // * NOTE: Same as above
        //case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
        //    return !get_extended ? "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS" :"VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS A buffer creation or memory allocation failed because the requested address is not available. A shader group handle assignment failed because the requested shader group handle information is no longer valid.";
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
        return !get_extended ? "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT" : "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT An operation on a swapchain created with VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT failed as it did not have exlusive full-screen access. This may occur due to implementation-dependent reasons, outside of the application�s control.";
    case VK_ERROR_UNKNOWN:
        return !get_extended ? "VK_ERROR_UNKNOWN" : "VK_ERROR_UNKNOWN An unknown error has occurred; either the application has provided invalid input, or an implementation failure has occurred.";
    }
}

b8 vulkan_result_is_success(VkResult result) {
    // From: https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkResult.html
    switch (result) {
        // Success Codes
    default:
    case VK_SUCCESS:
    case VK_NOT_READY:
    case VK_TIMEOUT:
    case VK_EVENT_SET:
    case VK_EVENT_RESET:
    case VK_INCOMPLETE:
    case VK_SUBOPTIMAL_KHR:
    case VK_THREAD_IDLE_KHR:
    case VK_THREAD_DONE_KHR:
    case VK_OPERATION_DEFERRED_KHR:
    case VK_OPERATION_NOT_DEFERRED_KHR:
    case VK_PIPELINE_COMPILE_REQUIRED_EXT:
        return TRUE;

        // Error codes
    case VK_ERROR_OUT_OF_HOST_MEMORY:
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
    case VK_ERROR_INITIALIZATION_FAILED:
    case VK_ERROR_DEVICE_LOST:
    case VK_ERROR_MEMORY_MAP_FAILED:
    case VK_ERROR_LAYER_NOT_PRESENT:
    case VK_ERROR_EXTENSION_NOT_PRESENT:
    case VK_ERROR_FEATURE_NOT_PRESENT:
    case VK_ERROR_INCOMPATIBLE_DRIVER:
    case VK_ERROR_TOO_MANY_OBJECTS:
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
    case VK_ERROR_FRAGMENTED_POOL:
    case VK_ERROR_SURFACE_LOST_KHR:
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
    case VK_ERROR_OUT_OF_DATE_KHR:
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
    case VK_ERROR_INVALID_SHADER_NV:
    case VK_ERROR_OUT_OF_POOL_MEMORY:
    case VK_ERROR_INVALID_EXTERNAL_HANDLE:
    case VK_ERROR_FRAGMENTATION:
    case VK_ERROR_INVALID_DEVICE_ADDRESS_EXT:
        // * NOTE: Same as above
        //case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
    case VK_ERROR_UNKNOWN:
        return FALSE;
    }
}