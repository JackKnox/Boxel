#pragma once

#include "defines.h"
#include "vulkan_types.h"

// Allocates one or more Vulkan command buffers from the given command pool.
VkResult vulkan_command_buffer_allocate(
    vulkan_context* context,
    vulkan_queue* owner,
    b8 is_primary,
    vulkan_command_buffer* command_buffer);

// Frees previously allocated Vulkan command buffers.
void vulkan_command_buffer_free(
    vulkan_context* context,
    vulkan_command_buffer* command_buffer);

// Begins recording commands into a command buffer.
VkResult vulkan_command_buffer_begin(
    vulkan_command_buffer* command_buffer,
    b8 is_single_use,
    b8 is_renderpass_continue,
    b8 is_simultaneous_use);

// Ends recording of a command buffer.
VkResult vulkan_command_buffer_end(
    vulkan_command_buffer* command_buffer);

// Submits a command buffer to its associated queue for execution.
VkResult vulkan_command_buffer_submit(
    vulkan_command_buffer* command_buffer,
    u32 wait_semaphore_count, VkSemaphore* wait_semaphores,
    u32 signal_semaphore_count, VkSemaphore* signal_semaphores,
    VkPipelineStageFlags* wait_stages,
    VkFence fence);

// Resets a command buffer to the initial state.
void vulkan_command_buffer_reset(
    vulkan_command_buffer* command_buffer);

// Allocates and begins recording a single-use primary command buffer.
VkResult vulkan_command_buffer_allocate_and_begin_single_use(
    vulkan_context* context,
    vulkan_queue* owner,
    vulkan_command_buffer* out_command_buffer);

// Ends, submits, waits for completion, and frees a single-use command buffer.
VkResult vulkan_command_buffer_end_single_use(
    vulkan_context* context,
    vulkan_command_buffer* command_buffer);