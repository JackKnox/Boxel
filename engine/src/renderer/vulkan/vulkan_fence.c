#include "defines.h"
#include "vulkan_fence.h"

VkResult vulkan_fence_create(
    vulkan_context* context,
    b8 create_signaled,
    vulkan_fence* out_fence) {

    // Make sure to signal the fence if required.
    out_fence->is_signaled = create_signaled;
    VkFenceCreateInfo fence_create_info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    if (out_fence->is_signaled) {
        fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    }

    return vkCreateFence(
        context->device.logical_device,
        &fence_create_info,
        context->allocator,
        &out_fence->handle);
}

void vulkan_fence_destroy(vulkan_context* context, vulkan_fence* fence) {
    if (fence && fence->handle) {
        vkDestroyFence(
            context->device.logical_device,
            fence->handle,
            context->allocator);

        fence->handle = 0;
        fence->is_signaled = FALSE;
    }
}

b8 vulkan_fence_wait(vulkan_context* context, vulkan_fence* fence, u64 timeout_ns) {
    if (fence->is_signaled) return TRUE;
    
    CHECK_VKRESULT(
        vkWaitForFences(
            context->device.logical_device,
            1,
            &fence->handle,
            TRUE,
            timeout_ns),
        "Failed to wait on Vulkan fence");

    fence->is_signaled = TRUE;
    return TRUE;
}

void vulkan_fence_reset(vulkan_context* context, vulkan_fence* fence) {
    if (fence->is_signaled) {
        VK_CHECK(vkResetFences(context->device.logical_device, 1, &fence->handle));
        fence->is_signaled = FALSE;
    }
}