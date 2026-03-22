#pragma once

#include "defines.h"

#include "renderer/renderer_backend.h"

#include "platform/vulkan_platform.h"

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

typedef enum vulkan_queue_type {
    VULKAN_QUEUE_TYPE_GRAPHICS,
    VULKAN_QUEUE_TYPE_COMPUTE,
    VULKAN_QUEUE_TYPE_TRANSFER,
    VULKAN_QUEUE_TYPE_PRESENT,
    VULKAN_QUEUE_TYPE_MAX,
} vulkan_queue_type;

// Represents a low level Vulkan image without a VkSampler.
typedef struct vulkan_image {
    VkImage handle;
    VkImageLayout layout;
    VkDeviceMemory memory;
    VkImageView view;
} vulkan_image;

// Represents a queue handle together with the command pool used to allocate command buffers for that queue family.
typedef struct vulkan_queue {
    VkQueue handle;
    VkCommandPool pool;
    box_renderer_mode supported_modes;
    i32 family_index;
} vulkan_queue;

// Represents a Vulkan command buffer and its current usage state.
typedef struct vulkan_command_buffer {
    VkCommandBuffer handle;
    vulkan_queue* owner;
} vulkan_command_buffer;

// Low level configuration for a attachment to a Vulkan-based rendertarget.
typedef struct vulkan_rendertarget_attachment {
    box_attachment_type type;
    vulkan_image* images;
    VkFormat format;
    VkSampleCountFlags samples;
    VkAttachmentLoadOp load_op;
    VkAttachmentStoreOp store_op;
    VkAttachmentLoadOp stencil_load_op;
    VkAttachmentStoreOp stencil_store_op;
    VkImageLayout final_layout;
} vulkan_rendertarget_attachment;

// Represents a logical Vulkan device and associated resources.
typedef struct vulkan_device {
    VkPhysicalDevice physical_device;
    VkDevice logical_device;
    vulkan_queue mode_queues[VULKAN_QUEUE_TYPE_MAX];
} vulkan_device;

// Represents a connection to a platform surface with swapcahin and synchronization primitives.
typedef struct vulkan_window_system {
    vulkan_image* images;
    u32 image_count;

    VkSemaphore* image_available_semaphores;
    VkFence** images_in_flight;

    VkSurfaceKHR surface;
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR* formats;
    VkPresentModeKHR* present_modes;

    VkSwapchainKHR swapchain;
    VkSurfaceFormatKHR swapchain_format;
} vulkan_window_system;

// Internal Vulkan implementation of a box_renderbuffer.
typedef struct internal_vulkan_renderbuffer {
    VkBuffer handle;
    VkDeviceMemory memory;
    VkBufferUsageFlags usage;
    VkMemoryPropertyFlags properties;
    VkMemoryRequirements memory_requirements;
} internal_vulkan_renderbuffer;

// Internal Vulkan implementation of a box_renderstage.
typedef struct internal_vulkan_renderstage {
    VkPipeline handle;
    VkPipelineLayout layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet* descriptor_sets;
    VkDescriptorSetLayout descriptor;

    union {
        struct {
            box_renderbuffer* vertex_buffer, * index_buffer;
        } graphics;
    };
} internal_vulkan_renderstage;

// Internal Vulkan implementation of a box_texture.
typedef struct internal_vulkan_texture {
    vulkan_image image;
    VkSampler sampler;
} internal_vulkan_texture;

// Internal Vulkan implementation of a box_rendertarget.
typedef struct internal_vulkan_rendertarget {
    VkRenderPass handle;
    vulkan_image* attachments;
    VkFramebuffer* framebuffers;
} internal_vulkan_rendertarget;

// Represents a relationship in resource memory between renderstages.
typedef struct memory_barrier {
    u64 created_on_submission;

    box_renderstage* src_renderstage;
    box_renderstage* dst_renderstage;
    box_access_flags src_access, dst_access;
} memory_barrier;

// Represents a queue submission to use directly with VkQueueSubmit.
typedef struct vulkan_queue_submission {
    vulkan_command_buffer* command_buffer;
    VkSemaphore signal_semaphore;
    VkSemaphore* wait_semaphores;
} vulkan_queue_submission;

// Represents the global Vulkan backend context.
// Owns the Vulkan instance, device, swapchain, synchronization primitives, and per-frame resources.
typedef struct vulkan_context {
    box_renderer_backend_config config;
    u32 current_frame, image_index;

    VkInstance instance;
    VkAllocationCallbacks* allocator;
    VkDebugUtilsMessengerEXT debug_messenger;
    vulkan_device device;
    
    vulkan_command_buffer* graphics_command_ring;
    vulkan_command_buffer* compute_command_ring;

    VkSemaphore* queue_complete_semaphores;
    VkFence* in_flight_fences;

    VkSemaphore* semaphore_pool;
    u32 semaphore_next_index;
    memory_barrier* memory_barriers;
    vulkan_queue_submission* queued_submissions;
    box_renderer_mode last_mode;
} vulkan_context;

// Finds a compatible memory type index on the physical device.
i32 find_memory_index(vulkan_context* context, u32 type_filter, VkMemoryPropertyFlags property_flags);

// Converts engine shader stage flags to Vulkan shader stage flags.
VkShaderStageFlags box_shader_type_to_vulkan_type(box_shader_stage_type type);

// Converts engine descriptor type to a Vulkan descriptor type.
VkDescriptorType box_descriptor_type_to_vulkan_type(box_descriptor_type descriptor_type);

// Converts engine filter mode to a Vulkan filter.
VkFilter box_filter_to_vulkan_type(box_filter_type filter_type);

// Converts engine address mode to a Vulkan sampler address mode.
VkSamplerAddressMode box_address_mode_to_vulkan_type(box_address_mode address);

// Converts engine render format to a Vulkan format.
VkFormat box_render_format_to_vulkan_type(box_render_format format);

// Converts engine load op format to a Vulkan format.
VkAttachmentLoadOp box_load_op_to_vulkan_type(box_load_op load_op);

// Converts engine store op format to a Vulkan format.
VkAttachmentStoreOp box_store_op_to_vulkan_type(box_store_op store_op);

// Returns a human-readable string for a Vulkan result code.
const char* vulkan_result_string(VkResult result, b8 get_extended);

// Determines whether a Vulkan result represents a success code.
b8 vulkan_result_is_success(VkResult result);