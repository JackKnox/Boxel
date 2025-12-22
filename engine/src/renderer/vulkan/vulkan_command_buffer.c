#include "defines.h"
#include "vulkan_command_buffer.h"

VkResult vulkan_command_buffer_allocate(
    vulkan_context* context,
    VkCommandPool pool,
    b8 is_primary,
    vulkan_command_buffer* out_command_buffer) {
    bzero_memory(out_command_buffer, sizeof(out_command_buffer));

    VkCommandBufferAllocateInfo allocate_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocate_info.level = is_primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    allocate_info.commandPool = pool;
    allocate_info.commandBufferCount = 1;

    out_command_buffer->owner = pool;
    out_command_buffer->state = COMMAND_BUFFER_STATE_NOT_ALLOCATED;

    VkResult result = vkAllocateCommandBuffers(context->device.logical_device, &allocate_info, &out_command_buffer->handle);
    if (!vulkan_result_is_success(result)) return result;

    out_command_buffer->state = COMMAND_BUFFER_STATE_READY;
    return TRUE;
}

void vulkan_command_buffer_free(
    vulkan_context* context,
    vulkan_command_buffer* command_buffer) {
    if (command_buffer && command_buffer->handle) {
        vkFreeCommandBuffers(context->device.logical_device, command_buffer->owner, 1, &command_buffer->handle);

        command_buffer->handle = 0;
        command_buffer->state = COMMAND_BUFFER_STATE_NOT_ALLOCATED;
    }
}

b8 vulkan_command_buffer_begin(
    vulkan_command_buffer* command_buffer,
    b8 is_single_use,
    b8 is_renderpass_continue,
    b8 is_simultaneous_use) {

    VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    if (is_single_use)
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (is_renderpass_continue)
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    if (is_simultaneous_use)
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    VkResult result = vkBeginCommandBuffer(command_buffer->handle, &begin_info);
    command_buffer->state = COMMAND_BUFFER_STATE_RECORDING;

    return vulkan_result_is_success(result);
}

void vulkan_command_buffer_end(vulkan_command_buffer* command_buffer) {
    VK_CHECK(vkEndCommandBuffer(command_buffer->handle));
    command_buffer->state = COMMAND_BUFFER_STATE_RECORDING_ENDED;
}

void vulkan_command_buffer_update_submitted(vulkan_command_buffer* command_buffer) {
    command_buffer->state = COMMAND_BUFFER_STATE_SUBMITTED;
}

void vulkan_command_buffer_reset(vulkan_command_buffer* command_buffer) {
    command_buffer->state = COMMAND_BUFFER_STATE_READY;
}

VkResult vulkan_command_buffer_allocate_and_begin_single_use(
    vulkan_context* context,
    VkCommandPool pool,
    vulkan_command_buffer* out_command_buffer) {
    VkResult result = vulkan_command_buffer_allocate(context, pool, TRUE, out_command_buffer);
    if (!vulkan_result_is_success(result)) return result;
    
    return vulkan_command_buffer_begin(out_command_buffer, TRUE, FALSE, FALSE) ? VK_SUCCESS : VK_ERROR_UNKNOWN;
}

VkResult vulkan_command_buffer_end_single_use(
    vulkan_context* context,
    vulkan_command_buffer* command_buffer,
    VkQueue queue) {

    // End the command buffer.
    vulkan_command_buffer_end(command_buffer);

    VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer->handle;

    // Submit the queue
    VkResult result = vkQueueSubmit(queue, 1, &submit_info, 0);
    if (!vulkan_result_is_success(result)) return result;

    // Wait for it to finish
    result = vkQueueWaitIdle(queue);
    if (!vulkan_result_is_success(result)) return result;

    // Free the command buffer.
    vulkan_command_buffer_free(context, command_buffer);
    return VK_SUCCESS;
}