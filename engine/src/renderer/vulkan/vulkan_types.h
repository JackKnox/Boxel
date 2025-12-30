#pragma once

#include "defines.h"

#include "utils/darray.h"

#include "renderer/renderer_backend.h"
#include "renderer/renderer_types.h"

#include <vulkan/vulkan.h>

// Checks the given expression's return value against VK_SUCCESS.
#define VK_CHECK(expr)                                    \
    {                                                     \
        VkResult r = expr;                                \
        if (!vulkan_result_is_success(r)) BX_FATAL("VK_CHECK failed: (Line = %i) "  __FILE__ "  (Error code: %s) ", __LINE__, vulkan_result_string(r, 1)); \
    }

#define CHECK_VKRESULT(func, message) { VkResult result = func; if (!vulkan_result_is_success(result)) { BX_ERROR(message ": %s", vulkan_result_string(result, BOX_ENABLE_VALIDATION)); return FALSE; } }

typedef enum vulkan_queue_type {
    VULKAN_QUEUE_TYPE_GRAPHICS, 
    VULKAN_QUEUE_TYPE_COMPUTE,
    VULKAN_QUEUE_TYPE_TRANSFER,
    VULKAN_QUEUE_TYPE_PRESENT,
    VULKAN_QUEUE_TYPE_MAX,
} vulkan_queue_type;

typedef struct vulkan_swapchain_support_info {
    VkSurfaceCapabilitiesKHR capabilities;
    // darray
    VkSurfaceFormatKHR* formats;
    // darray
    VkPresentModeKHR* present_modes;
} vulkan_swapchain_support_info;

typedef struct vulkan_queue {
    VkQueue handle;
    VkCommandPool pool;
    renderer_mode supported_modes;
    i32 family_index;
} vulkan_queue;

typedef struct vulkan_device {
    VkPhysicalDevice physical_device;
    VkDevice logical_device;
    vulkan_swapchain_support_info swapchain_support;

    vulkan_queue mode_queues[VULKAN_QUEUE_TYPE_MAX];
} vulkan_device;

typedef struct vulkan_image {
    VkImage handle;
    VkDeviceMemory memory;
    VkImageView view;
    uvec2 size;
} vulkan_image;

typedef struct vulkan_buffer {
    VkBuffer handle;
    VkDeviceMemory memory;
    VkBufferUsageFlags usage;
    VkMemoryPropertyFlags properties;
    VkMemoryRequirements memory_requirements;
} vulkan_buffer;

typedef enum vulkan_render_pass_state {
    RENDER_PASS_STATE_NOT_ALLOCATED,
    RENDER_PASS_STATE_READY,
    RENDER_PASS_STATE_RECORDING,
    RENDER_PASS_STATE_RECORDING_ENDED,
} vulkan_render_pass_state;

typedef struct vulkan_renderpass {
    VkRenderPass handle;
    vec2 origin, size;

    vulkan_render_pass_state state;
    u32 clear_colour;
} vulkan_renderpass;

typedef struct vulkan_framebuffer {
    VkFramebuffer handle;
    VkImageView* attachments;
    vulkan_renderpass* renderpass;
    u32 attachment_count;
} vulkan_framebuffer;

typedef struct vulkan_pipeline {
    VkPipeline handle;
    VkPipelineLayout layout;

    VkDescriptorSetLayout descriptor;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet* descriptor_sets;
    VkPipelineBindPoint bind_point;
} vulkan_pipeline;

typedef struct vulkan_swapchain {
    VkSurfaceFormatKHR image_format;
    VkSwapchainKHR handle;

    VkImage* images;
    VkImageView* views;
    u32 image_count;

    // framebuffers used for on-screen rendering.
    vulkan_framebuffer* framebuffers;
} vulkan_swapchain;

typedef enum vulkan_command_buffer_state {
    COMMAND_BUFFER_STATE_READY,
    COMMAND_BUFFER_STATE_RECORDING,
    COMMAND_BUFFER_STATE_IN_RENDER_PASS,
    COMMAND_BUFFER_STATE_RECORDING_ENDED,
    COMMAND_BUFFER_STATE_SUBMITTED,
    COMMAND_BUFFER_STATE_NOT_ALLOCATED
} vulkan_command_buffer_state;

typedef struct vulkan_command_buffer {
    VkCommandBuffer handle;
    VkCommandPool owner;

    // Command buffer state.
    vulkan_command_buffer_state state;
    b8 used;
} vulkan_command_buffer;

typedef struct vulkan_fence {
    VkFence handle;
    b8 is_signaled;
} vulkan_fence;

typedef struct vulkan_context {
    vec2 framebuffer_size;
    u32 image_index;
    u32 current_frame;

    VkInstance instance;
    VkAllocationCallbacks* allocator;
    VkSurfaceKHR surface;

    VkDebugUtilsMessengerEXT debug_messenger;

    vulkan_device device;
    vulkan_swapchain swapchain;
    vulkan_renderpass main_renderpass;

    // darray
    vulkan_command_buffer* command_buffer_ring[2];
    // darray
    VkSemaphore* image_available_semaphores;
    // darray
    VkSemaphore* queue_complete_semaphores;
    // darray
    vulkan_fence* in_flight_fences;
    // darray - Holds pointers to fences which exist and are owned elsewhere.
    vulkan_fence** images_in_flight;
} vulkan_context;

i32 find_memory_index(vulkan_context* context, u32 type_filter, u32 property_flags);

VkShaderStageFlags box_shader_type_to_vulkan_type(box_shader_stage_type type);

VkDescriptorType box_renderbuffer_usage_to_vulkan_type(box_renderbuffer_usage usage);

VkIndexType box_data_type_to_vulkan_index_type(box_render_format data_type);

VkFormat box_attribute_to_vulkan_type(box_render_format type, u64 count);

const char* vulkan_result_string(VkResult result, b8 get_extended);

b8 vulkan_result_is_success(VkResult result);