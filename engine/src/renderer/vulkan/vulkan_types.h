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

// Types of Vulkan queues used by the renderer backend.
typedef enum {
    // Queue capable of graphics operations such as draw calls, render passes, and graphics pipeline execution.
    VULKAN_QUEUE_TYPE_GRAPHICS,

    // Queue capable of compute dispatches and compute pipeline execution.
    VULKAN_QUEUE_TYPE_COMPUTE,

    // Queue dedicated to transfer operations such as buffer and image copies.
    VULKAN_QUEUE_TYPE_TRANSFER,

    // Queue capable of presenting rendered images to a surface.
    VULKAN_QUEUE_TYPE_PRESENT,

    VULKAN_QUEUE_TYPE_MAX,
} vulkan_queue_type;

// Swapchain capability information queried from the physical device.
typedef struct {
    // Surface capabilities (extent, image count limits, transforms, etc).
    VkSurfaceCapabilitiesKHR capabilities;

    // Supported surface formats (darray).
    VkSurfaceFormatKHR* formats;

    // Supported presentation modes (darray).
    VkPresentModeKHR* present_modes;
} vulkan_swapchain_support_info;

// Represents a logical queue and its associated command pool.
typedef struct {
    // Vulkan queue handle.
    VkQueue handle;

    // Command pool owned by this queue family.
    VkCommandPool pool;

    // Renderer modes supported by this queue (graphics / compute).
    box_renderer_mode supported_modes;

    // Queue family index.
    i32 family_index;
} vulkan_queue;

// Encapsulates Vulkan device state and queue ownership.
typedef struct {
    // Selected physical device.
    VkPhysicalDevice physical_device;

    // Logical device created from the physical device.
    VkDevice logical_device;

    // Cached swapchain support details.
    vulkan_swapchain_support_info swapchain_support;

    // Queues grouped by intended usage.
    vulkan_queue mode_queues[VULKAN_QUEUE_TYPE_MAX];
} vulkan_device;

// Backend representation of an image resource.
typedef struct {
    VkImage handle;
    VkImageLayout layout;
    VkDeviceMemory memory;
    VkImageView view;
    VkSampler sampler;

    // Dimensions of the image.
    uvec2 size;
} vulkan_image;

// Backend representation of a buffer resource.
typedef struct {
    VkBuffer handle;
    VkDeviceMemory memory;

    // Vulkan usage flags (vertex, index, uniform, etc).
    VkBufferUsageFlags usage;

    // Memory property flags (host-visible, device-local, etc).
    VkMemoryPropertyFlags properties;

    // Cached memory requirements for allocation.
    VkMemoryRequirements memory_requirements;
} vulkan_buffer;

// Render pass lifecycle states.
typedef enum {
    // Render pass has not been created or has been explicitly destroyed.
    RENDER_PASS_STATE_NOT_ALLOCATED,

    // Render pass is created and ready to be begun.
    RENDER_PASS_STATE_READY,

    // Render pass is actively recording commands.
    RENDER_PASS_STATE_RECORDING,

    // Render pass recording has ended.
    RENDER_PASS_STATE_RECORDING_ENDED,
} vulkan_render_pass_state;

// Backend representation of a Vulkan render pass.
typedef struct {
    VkRenderPass handle;

    // Render area origin and size.
    vec2 origin, size;

    // Current render pass state.
    vulkan_render_pass_state state;

    // Packed clear colour used at render pass begin.
    u32 clear_colour;
} vulkan_renderpass;

// Framebuffer associated with a render pass.
typedef struct {
    VkFramebuffer handle;

    // Image views used as framebuffer attachments.
    VkImageView* attachments;

    // Owning render pass.
    vulkan_renderpass* renderpass;

    // Number of attachments.
    u32 attachment_count;
} vulkan_framebuffer;

// Backend representation of a graphics or compute pipeline.
typedef struct {
    VkPipeline handle;
    VkPipelineLayout layout;

    VkDescriptorSetLayout descriptor;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet* descriptor_sets;
    VkPipelineBindPoint bind_point;
} vulkan_pipeline;

// Swapchain and its associated image resources.
typedef struct {
    VkSurfaceFormatKHR image_format;
    VkSwapchainKHR handle;

    // Swapchain images and image views (darray).
    VkImage* images;
    VkImageView* views;
    u32 image_count;

    // Framebuffers used for on-screen rendering.
    vulkan_framebuffer* framebuffers;
} vulkan_swapchain;

// Command buffer lifecycle states.
typedef enum {
    // Command buffer is allocated and reset, ready to begin recording.
    COMMAND_BUFFER_STATE_READY,

    // Command buffer is actively recording commands via vkBeginCommandBuffer.
    COMMAND_BUFFER_STATE_RECORDING,

    // Command buffer is currently inside an active render pass.
    COMMAND_BUFFER_STATE_IN_RENDER_PASS,

    // Command buffer recording has finished via vkEndCommandBuffer.
    COMMAND_BUFFER_STATE_RECORDING_ENDED,

    // Command buffer has been submitted to a queue for execution.
    COMMAND_BUFFER_STATE_SUBMITTED,

    // Command buffer has not been allocated or has been explicitly freed.
    COMMAND_BUFFER_STATE_NOT_ALLOCATED
} vulkan_command_buffer_state;

// Wrapper around a Vulkan command buffer with state tracking.
typedef struct {
    VkCommandBuffer handle;

    // Command pool which owns this buffer.
    VkCommandPool owner;

    // Current state of the command buffer.
    vulkan_command_buffer_state state;

    // Indicates whether the buffer has been used this frame.
    b8 used;
} vulkan_command_buffer;

// Fence wrapper with cached signal state.
typedef struct {
    VkFence handle;
    b8 is_signaled;
} vulkan_fence;

// Global Vulkan renderer context.
typedef struct {
    // Backend configuration provided by the engine.
    box_renderer_backend_config config;

    // Current framebuffer dimensions.
    vec2 framebuffer_size;

    // Current swapchain image index.
    u32 image_index;

    // Frame-in-flight index.
    u32 current_frame;

    VkInstance instance;
    VkAllocationCallbacks* allocator;
    VkSurfaceKHR surface;

    // Debug messenger (validation layers).
    VkDebugUtilsMessengerEXT debug_messenger;

    vulkan_device device;
    vulkan_swapchain swapchain;
    vulkan_renderpass main_renderpass;

    // Command buffers per frame (darray).
    vulkan_command_buffer* command_buffer_ring[2];

    // Synchronization primitives (darrays).
    VkSemaphore* image_available_semaphores;
    VkSemaphore* queue_complete_semaphores;
    vulkan_fence* in_flight_fences;

    // Tracks which fence is associated with each swapchain image (darray).
    // Pointers refer to fences owned elsewhere.
    vulkan_fence** images_in_flight;
} vulkan_context;

// Finds a compatible memory type index on the physical device.
i32 find_memory_index(vulkan_context* context, u32 type_filter, u32 property_flags);

// Converts engine shader stage flags to Vulkan shader stage flags.
VkShaderStageFlags box_shader_type_to_vulkan_type(box_shader_stage_type type);

// Converts engine descriptor type to Vulkan descriptor type.
VkDescriptorType box_descriptor_type_to_vulkan_type(box_descriptor_type descriptor_type);

// Converts engine index format to Vulkan index type.
VkIndexType box_format_to_vulkan_index_type(box_format_type data_type);

// Converts engine filter mode to Vulkan filter.
VkFilter box_filter_to_vulkan_type(box_filter_type filter_type);

// Converts engine address mode to Vulkan sampler address mode.
VkSamplerAddressMode box_address_mode_to_vulkan_type(box_address_mode address);

// Converts engine render format to Vulkan format.
VkFormat box_format_to_vulkan_type(box_render_format format);

// Returns a human-readable string for a Vulkan result code.
const char* vulkan_result_string(VkResult result, b8 get_extended);

// Returns true if the Vulkan result represents a success code.
b8 vulkan_result_is_success(VkResult result);