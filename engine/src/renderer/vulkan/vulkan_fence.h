#pragma once

#include "defines.h"
#include "vulkan_types.h"

/**
 * @brief Creates a Vulkan fence.
 *
 * Fences are used to synchronize CPU and GPU execution, typically to detect
 * when submitted work has completed.
 *
 * @param context Pointer to the Vulkan context.
 * @param create_signaled Indicates whether the fence should be created in the signaled state.
 * @param out_fence Pointer that receives the created fence.
 *
 * @return VkResult indicating success or failure.
 */
VkResult vulkan_fence_create(
    vulkan_context* context,
    b8 create_signaled,
    vulkan_fence* out_fence);

/**
 * @brief Destroys a Vulkan fence.
 *
 * Releases the underlying Vulkan fence handle and associated resources.
 *
 * @param context Pointer to the Vulkan context.
 * @param fence Pointer to the fence to destroy.
 */
void vulkan_fence_destroy(
    vulkan_context* context,
    vulkan_fence* fence);

/**
 * @brief Waits for a fence to become signaled.
 *
 * Blocks the calling thread until the fence is signaled or the timeout expires.
 *
 * @param context Pointer to the Vulkan context.
 * @param fence Pointer to the fence to wait on.
 * @param timeout_ns Timeout in nanoseconds. Use 0 for an infinite wait.
 *
 * @return True if the fence was successfully signaled, false if the wait timed out or failed.
 */
b8 vulkan_fence_wait(
    vulkan_context* context,
    vulkan_fence* fence,
    u64 timeout_ns);

/**
 * @brief Resets a fence to the unsignaled state.
 *
 * The fence must not be in a pending wait operation.
 *
 * @param context Pointer to the Vulkan context.
 * @param fence Pointer to the fence to reset.
 */
void vulkan_fence_reset(
    vulkan_context* context,
    vulkan_fence* fence);
