#include "defines.h"
#include "vulkan_command_buffer.h"

VkResult vulkan_command_buffer_allocate(
    vulkan_context* context,
    vulkan_queue* owner,
    b8 is_primary,
    vulkan_command_buffer* out_command_buffer) {
    bzero_memory(out_command_buffer, sizeof(vulkan_command_buffer));

    VkCommandBufferAllocateInfo allocate_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocate_info.level = is_primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    allocate_info.commandPool = owner->pool;
    allocate_info.commandBufferCount = 1;

    out_command_buffer->state = COMMAND_BUFFER_STATE_NOT_ALLOCATED;

    VkResult result = vkAllocateCommandBuffers(context->device.logical_device, &allocate_info, &out_command_buffer->handle);
    if (!vulkan_result_is_success(result)) return result;

    out_command_buffer->owner = owner;
    out_command_buffer->state = COMMAND_BUFFER_STATE_READY;
    return VK_SUCCESS;
}

void vulkan_command_buffer_free(
    vulkan_context* context,
    vulkan_command_buffer* command_buffer) {
    if (command_buffer && command_buffer->handle) {
        vkFreeCommandBuffers(context->device.logical_device, command_buffer->owner->pool, 1, &command_buffer->handle);

        command_buffer->handle = 0;
        command_buffer->state = COMMAND_BUFFER_STATE_NOT_ALLOCATED;
    }
}

VkResult vulkan_command_buffer_begin(
    vulkan_command_buffer* command_buffer,
    b8 is_single_use,
    b8 is_renderpass_continue,
    b8 is_simultaneous_use) {
    VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    if (is_single_use)          begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (is_renderpass_continue) begin_info.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    if (is_simultaneous_use)    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    VkResult result = vkBeginCommandBuffer(
            command_buffer->handle, 
            &begin_info);
    if (!vulkan_result_is_success(result)) return result;

    command_buffer->state = COMMAND_BUFFER_STATE_RECORDING;
    return VK_SUCCESS;
}

VkResult vulkan_command_buffer_end(vulkan_command_buffer* command_buffer) {
    VkResult result = vkEndCommandBuffer(command_buffer->handle);
    if (!vulkan_result_is_success(result)) return result;

    command_buffer->state = COMMAND_BUFFER_STATE_RECORDING_ENDED;
    return VK_SUCCESS;
}

VkResult vulkan_command_buffer_submit(
    vulkan_command_buffer* command_buffer, 
    u32 wait_semaphore_count, VkSemaphore* wait_semaphores, 
    u32 signal_semaphore_count, VkSemaphore* signal_semaphores,
    VkPipelineStageFlags* wait_stages, 
    VkFence fence) {
        
	VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit_info.pCommandBuffers = &command_buffer->handle;
    submit_info.commandBufferCount = 1;
    submit_info.waitSemaphoreCount = wait_semaphore_count;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.signalSemaphoreCount = signal_semaphore_count;
    submit_info.pSignalSemaphores = signal_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;

    VkResult result = vkQueueSubmit(command_buffer->owner->handle, 1, &submit_info, fence);
    if (!vulkan_result_is_success(result)) return result;

    command_buffer->state = COMMAND_BUFFER_STATE_SUBMITTED;
    return VK_SUCCESS;
}

void vulkan_command_buffer_reset(vulkan_command_buffer *command_buffer) {
    command_buffer->state = COMMAND_BUFFER_STATE_READY;
}

VkResult vulkan_command_buffer_allocate_and_begin_single_use(
    vulkan_context* context,
    vulkan_queue* owner,
    vulkan_command_buffer* out_command_buffer) {
    VkResult result = vulkan_command_buffer_allocate(context, owner, TRUE, out_command_buffer);
    if (!vulkan_result_is_success(result)) return result;
    
    return vulkan_command_buffer_begin(out_command_buffer, TRUE, FALSE, FALSE);
}

VkResult vulkan_command_buffer_end_single_use(
    vulkan_context* context,
    vulkan_command_buffer* command_buffer) {
    // End the command buffer.
    vulkan_command_buffer_end(command_buffer);

    VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer->handle;

    // Submit the queue
    VkResult result = vkQueueSubmit(command_buffer->owner->handle, 1, &submit_info, 0);
    if (!vulkan_result_is_success(result)) return result;

    // Wait for it to finish
    result = vkQueueWaitIdle(command_buffer->owner->handle);
    if (!vulkan_result_is_success(result)) return result;

    // Free the command buffer.
    vulkan_command_buffer_free(context, command_buffer);
    return VK_SUCCESS;
}