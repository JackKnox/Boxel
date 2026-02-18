#pragma once

#include "defines.h"

#include "renderer/renderer_backend.h"

#include <vulkan/vulkan.h>

// Checks the given Vulkan expression for success and fatally aborts on failure.
// Intended for calls that must never fail in a valid engine state.
#define VK_CHECK(expr)                                    \
    {                                                     \
        VkResult r = expr;                                \
        if (!vulkan_result_is_success(r))                 \
            BX_FATAL("VK_CHECK failed: (Line = %i) " __FILE__ "  (Error code: %s) ", \
                     __LINE__, vulkan_result_string(r, 1)); \
    }

// Checks a Vulkan call and logs an error on failure.
// Intended for recoverable errors during initialization or runtime.
#define CHECK_VKRESULT(func, message)                     \
    {                                                     \
        VkResult result = func;                           \
        if (!vulkan_result_is_success(result)) {          \
            BX_ERROR(message ": %s",                      \
                     vulkan_result_string(result, BOX_ENABLE_VALIDATION)); \
            return FALSE;                                 \
        }                                                 \
    }

/**
 * @brief Identifies the different Vulkan queue roles used by the backend.
 *
 * Each queue type corresponds to a queue family that supports a specific
 * class of operations. Not all physical devices expose separate families
 * for each role.
 */
typedef enum vulkan_queue_type {
    /** @brief Queue capable of graphics operations such as draw calls, render passes, and graphics pipeline execution. */
    VULKAN_QUEUE_TYPE_GRAPHICS,

    /** @brief Queue capable of compute dispatches and compute pipeline execution. */
    VULKAN_QUEUE_TYPE_COMPUTE,

    /** @brief Queue dedicated to transfer operations such as buffer and image copies. */
    VULKAN_QUEUE_TYPE_TRANSFER,

    /** @brief Queue capable of presenting rendered images to a surface. */
    VULKAN_QUEUE_TYPE_PRESENT,

    VULKAN_QUEUE_TYPE_MAX,
} vulkan_queue_type;

/**
 * @brief Stores swapchain support details for a physical device.
 *
 * Queried during device selection to determine swapchain compatibility
 * with a given surface.
 */
typedef struct vulkan_swapchain_support_info {
     /** @brief Surface capabilities. */
    VkSurfaceCapabilitiesKHR capabilities;

    /** @brief Supported surface formats. */
    VkSurfaceFormatKHR* formats;

    /** @brief Supported presentation modes. */
    VkPresentModeKHR* present_modes;
} vulkan_swapchain_support_info;

/**
 * @brief Represents a Vulkan queue and its associated command pool.
 *
 * Wraps a VkQueue handle together with the command pool used to allocate
 * command buffers for that queue family.
 */
typedef struct vulkan_queue {
    /** @brief Vulkan queue handle. */
    VkQueue handle;

    /** @brief Command pool associated with this queue family. */
    VkCommandPool pool;

    /** @brief Supported renderer modes for this queue. */
    box_renderer_mode supported_modes;

    /** @brief Queue family index. */
    i32 family_index;
} vulkan_queue;

/**
 * @brief Lifecycle state of a Vulkan render pass instance.
 */
typedef enum vulkan_render_pass_state {
    /** @brief Render pass has not been allocated. */
    RENDER_PASS_STATE_NOT_ALLOCATED,

    /** @brief Render pass is ready for recording. */
    RENDER_PASS_STATE_READY,

    /** @brief Render pass is currently recording commands. */
    RENDER_PASS_STATE_RECORDING,

    /** @brief Recording has ended. */
    RENDER_PASS_STATE_RECORDING_ENDED,
} vulkan_render_pass_state;

/**
 * @brief Wraps a VkFramebuffer and its attachments.
 */
typedef struct vulkan_framebuffer {
    /** @brief Framebuffer handle. */
    VkFramebuffer handle;

    /** @brief Image view attachments. */
    VkImageView* attachments;

    /** @brief Number of attachments. */
    u32 attachment_count;
} vulkan_framebuffer;

/**
 * @brief Represents the Vulkan swapchain and its associated images.
 */
typedef struct vulkan_swapchain {
    /** @brief Selected surface format. */
    VkSurfaceFormatKHR image_format;

    /** @brief Swapchain handle. */
    VkSwapchainKHR handle;

    /** @brief Swapchain images. */
    VkImage* images;

    /** @brief Image views for swapchain images. */
    VkImageView* views;

    /** @brief Number of swapchain images. */
    u32 image_count;
} vulkan_swapchain;

/**
 * @brief Tracks the state of a Vulkan command buffer.
 */
typedef enum vulkan_command_buffer_state {
    /** @brief Ready for recording. */
    COMMAND_BUFFER_STATE_READY,

    /** @brief Currently recording. */
    COMMAND_BUFFER_STATE_RECORDING,

    /** @brief Inside a render pass. */
    COMMAND_BUFFER_STATE_IN_RENDER_PASS,

    /** @brief Recording has ended. */
    COMMAND_BUFFER_STATE_RECORDING_ENDED,

    /** @brief Submitted to a queue. */
    COMMAND_BUFFER_STATE_SUBMITTED,

    /** @brief Not allocated. */
    COMMAND_BUFFER_STATE_NOT_ALLOCATED
} vulkan_command_buffer_state;

/**
 * @brief Wraps a Vulkan command buffer and tracks its usage state.
 */
typedef struct vulkan_command_buffer {
    /** @brief Command buffer handle. */
    VkCommandBuffer handle;

    /** @brief Owning command pool. */
    VkCommandPool owner;

    /** @brief Current command buffer state. */
    vulkan_command_buffer_state state;

    /** @brief Indicates whether the buffer has been used this frame. */
    b8 used;
} vulkan_command_buffer;

/**
 * @brief Wraps a Vulkan fence used for GPU/CPU synchronization.
 */
typedef struct vulkan_fence {
    /** @brief Fence handle. */
    VkFence handle;

    /** @brief Cached signal state. */
    b8 is_signaled;
} vulkan_fence;

/**
 * @brief Represents a logical Vulkan device and associated resources.
 */
typedef struct vulkan_device {
    /** @brief Selected physical device. */
    VkPhysicalDevice physical_device;

    /** @brief Logical device handle. */
    VkDevice logical_device;

    /** @brief Swapchain support details. */
    vulkan_swapchain_support_info swapchain_support;

    /** @brief Queues indexed by vulkan_queue_type. */
    vulkan_queue mode_queues[VULKAN_QUEUE_TYPE_MAX];
} vulkan_device;

/**
 * @brief Internal Vulkan implementation of a box_renderbuffer.
 */
typedef struct internal_vulkan_renderbuffer {
    /** @brief Buffer handle. */
    VkBuffer handle;

    /** @brief Allocated device memory. */
    VkDeviceMemory memory;

    /** @brief Buffer usage flags. */
    VkBufferUsageFlags usage;

    /** @brief Memory property flags. */
    VkMemoryPropertyFlags properties;

    /** @brief Memory requirements for allocation. */
    VkMemoryRequirements memory_requirements;
} internal_vulkan_renderbuffer;

/**
 * @brief Internal Vulkan implementation of a box_renderstage.
 */
typedef struct internal_vulkan_renderstage {
    /** @brief Pipeline handle. */
    VkPipeline handle;

    /** @brief Pipeline layout. */
    VkPipelineLayout layout;

    /** @brief Descriptor pool. */
    VkDescriptorPool descriptor_pool;

    /** @brief Allocated descriptor sets. */
    VkDescriptorSet* descriptor_sets;

    /** @brief Descriptor set layout. */
    VkDescriptorSetLayout descriptor;
} internal_vulkan_renderstage;

/**
 * @brief Internal Vulkan implementation of a box_texture.
 */
typedef struct internal_vulkan_texture {
    /** @brief Image handle. */
    VkImage handle;

    /** @brief Current image layout. */
    VkImageLayout layout;

    /** @brief Allocated device memory. */
    VkDeviceMemory memory;

    /** @brief Image view. */
    VkImageView view;

    /** @brief Sampler object. */
    VkSampler sampler;
} internal_vulkan_texture;

/**
 * @brief Internal Vulkan render target implementation.
 *
 * Wraps a render pass and its framebuffers.
 */
typedef struct internal_vulkan_rendertarget {
    /** @brief Render pass handle. */
    VkRenderPass handle;

    /** @brief Current render pass state. */
    vulkan_render_pass_state state;

    /** @brief Associated framebuffers. */
    vulkan_framebuffer* framebuffers;
} internal_vulkan_rendertarget;

/**
 * @brief Global Vulkan backend context.
 *
 * Owns the Vulkan instance, device, swapchain, synchronization primitives,
 * and per-frame resources. One context exists per renderer backend instance.
 */
typedef struct vulkan_context {
    /** @brief Backend configuration. */
    box_renderer_backend_config config;

    /** @brief Has the backend rendered anything this frame? */
    b8 rendered_this_frame;

    /** @brief Current framebuffer size. */
    vec2 framebuffer_size;

    /** @brief Current swapchain image index. */
    u32 image_index;

    /** @brief Current frame-in-flight index. */
    u32 current_frame;

    /** @brief Vulkan instance handle. */
    VkInstance instance;

    /** @brief Custom allocator callbacks (optional). */
    VkAllocationCallbacks* allocator;

    /** @brief Presentation surface. */
    VkSurfaceKHR surface;

    /** @brief Debug messenger for validation layers. */
    VkDebugUtilsMessengerEXT debug_messenger;

    /** @brief Logical device and queues. */
    vulkan_device device;

    /** @brief Swapchain. */
    vulkan_swapchain swapchain;

    /** @brief Per-frame command buffers. */
    vulkan_command_buffer* command_buffer_ring[2];

    /** @brief Image-available semaphores. */
    VkSemaphore* image_available_semaphores;

    /** @brief Render-complete semaphores. */
    VkSemaphore* queue_complete_semaphores;

    /** @brief Per-frame fences. */
    vulkan_fence* in_flight_fences;

    /** @brief Fence tracking per swapchain image. */
    vulkan_fence** images_in_flight;
} vulkan_context;

/**
 * @brief Creates a CPU-visible staging buffer and optionally uploads data to it.
 *
 * Typically used for transferring data (e.g., vertex/index/texture data)
 * to device-local GPU buffers via a copy operation.
 *
 * @param context Pointer to the Vulkan context.
 * @param buf_size Size of the buffer in bytes.
 * @param map_ptr Optional pointer to data to copy into the buffer (can be NULL).
 * @param out_buffer Pointer that receives the created render buffer.
 *
 * @return True if creation succeeded, otherwise false.
 */
b8 create_staging_buffer(
    vulkan_context* context,
    const void* map_ptr,
    box_renderbuffer* out_buffer);

/**
 * @brief Finds a compatible memory type index on the physical device.
 *
 * Searches the physical device memory properties for a memory type
 * that satisfies the given type filter and property flags.
 *
 * @param context Pointer to the Vulkan context.
 * @param type_filter Bitmask of acceptable memory types.
 * @param property_flags Required memory property flags.
 *
 * @return The matching memory type index, or -1 if none is found.
 */
i32 find_memory_index(
    vulkan_context* context,
    u32 type_filter,
    u32 property_flags);

/**
 * @brief Converts engine shader stage flags to Vulkan shader stage flags.
 */
VkShaderStageFlags box_shader_type_to_vulkan_type(box_shader_stage_type type);

/**
 * @brief Converts engine descriptor type to a Vulkan descriptor type.
 */
VkDescriptorType box_descriptor_type_to_vulkan_type(box_descriptor_type descriptor_type);

/**
 * @brief Converts engine index format to a Vulkan index type.
 */
VkIndexType box_format_to_vulkan_index_type(box_format_type data_type);

/**
 * @brief Converts engine filter mode to a Vulkan filter.
 */
VkFilter box_filter_to_vulkan_type(box_filter_type filter_type);

/**
 * @brief Converts engine address mode to a Vulkan sampler address mode.
 */
VkSamplerAddressMode box_address_mode_to_vulkan_type(box_address_mode address);

/**
 * @brief Converts engine render format to a Vulkan format.
 */
VkFormat box_format_to_vulkan_type(box_render_format format);

/**
 * @brief Returns a human-readable string for a Vulkan result code.
 *
 * @param result Vulkan result code.
 * @param get_extended If true, returns a more descriptive message when available.
 *
 * @return Constant string describing the result.
 */
const char* vulkan_result_string(VkResult result, b8 get_extended);

/**
 * @brief Determines whether a Vulkan result represents a success code.
 */
b8 vulkan_result_is_success(VkResult result);