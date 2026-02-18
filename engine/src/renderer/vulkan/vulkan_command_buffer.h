#pragma once

#include "defines.h"
#include "vulkan_types.h"

/**
 * @brief Allocates one or more Vulkan command buffers from the given command pool.
 *
 * @param context Pointer to the Vulkan context.
 * @param pool The command pool to allocate from.
 * @param is_primary Indicates whether to allocate primary (true) or secondary (false) command buffers.
 * @param command_buffer Pointer that receives the allocated command buffer.
 *
 * @return VkResult indicating success or failure.
 */
VkResult vulkan_command_buffer_allocate(
    vulkan_context* context,
    VkCommandPool pool,
    b8 is_primary,
    vulkan_command_buffer* command_buffer);

/**
 * @brief Frees previously allocated Vulkan command buffers.
 *
 * @param context Pointer to the Vulkan context.
 * @param command_buffers Pointer to the command buffer to free.
 */
void vulkan_command_buffer_free(
    vulkan_context* context,
    vulkan_command_buffer* command_buffer);

/**
 * @brief Begins recording commands into a command buffer.
 *
 * @param command_buffer Pointer to the command buffer to begin.
 * @param is_single_use Indicates the command buffer will be submitted only once.
 * @param is_renderpass_continue Indicates this is a secondary buffer continuing a render pass.
 * @param is_simultaneous_use Indicates the buffer can be resubmitted while already pending execution.
 *
 * @return VkResult indicating success or failure.
 */
VkResult vulkan_command_buffer_begin(
    vulkan_command_buffer* command_buffer,
    b8 is_single_use,
    b8 is_renderpass_continue,
    b8 is_simultaneous_use);

/**
 * @brief Ends recording of a command buffer.
 *
 * @param command_buffer Pointer to the command buffer to end.
 */
void vulkan_command_buffer_end(
    vulkan_command_buffer* command_buffer);

/**
 * @brief Marks a command buffer as submitted.
 *
 * Typically used to update internal state tracking after queue submission.
 *
 * @param command_buffer Pointer to the command buffer.
 */
void vulkan_command_buffer_update_submitted(
    vulkan_command_buffer* command_buffer);

/**
 * @brief Resets a command buffer to the initial state.
 *
 * The command buffer must not be in a pending execution state.
 *
 * @param command_buffer Pointer to the command buffer to reset.
 */
void vulkan_command_buffer_reset(
    vulkan_command_buffer* command_buffer);

/**
 * @brief Allocates and begins recording a single-use primary command buffer.
 *
 * Convenience function for short-lived command buffers (e.g., transfers).
 *
 * @param context Pointer to the Vulkan context.
 * @param pool The command pool to allocate from.
 * @param out_command_buffer Pointer that receives the allocated command buffer.
 *
 * @return VkResult indicating success or failure.
 */
VkResult vulkan_command_buffer_allocate_and_begin_single_use(
    vulkan_context* context,
    VkCommandPool pool,
    vulkan_command_buffer* out_command_buffer);

/**
 * @brief Ends, submits, waits for completion, and frees a single-use command buffer.
 *
 * Intended to be used with vulkan_command_buffer_allocate_and_begin_single_use().
 *
 * @param context Pointer to the Vulkan context.
 * @param command_buffer Pointer to the command buffer to end and submit.
 * @param queue The queue to submit the command buffer to.
 *
 * @return VkResult indicating success or failure.
 */
VkResult vulkan_command_buffer_end_single_use(
    vulkan_context* context,
    vulkan_command_buffer* command_buffer,
    VkQueue queue);