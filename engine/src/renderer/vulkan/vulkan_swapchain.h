#pragma once

#include "defines.h"
#include "vulkan_types.h"

/**
 * @brief Creates a Vulkan swapchain.
 *
 * Initializes the swapchain, creates its images and associated image views
 * based on the provided size and surface capabilities.
 *
 * @param context Pointer to the Vulkan context.
 * @param size Desired width and height of the swapchain images.
 * @param out_swapchain Pointer that receives the created swapchain.
 *
 * @return VkResult indicating success or failure.
 */
VkResult vulkan_swapchain_create(
    vulkan_context* context,
    vec2 size,
    vulkan_swapchain* out_swapchain);

/**
 * @brief Recreates an existing Vulkan swapchain.
 *
 * Typically called when the window is resized or the swapchain becomes invalid.
 * Destroys and rebuilds swapchain-dependent resources.
 *
 * @param context Pointer to the Vulkan context.
 * @param size New width and height of the swapchain images.
 * @param swapchain Pointer to the swapchain to recreate.
 *
 * @return VkResult indicating success or failure.
 */
VkResult vulkan_swapchain_recreate(
    vulkan_context* context,
    vec2 size,
    vulkan_swapchain* swapchain);

/**
 * @brief Destroys a Vulkan swapchain and its associated resources.
 *
 * Frees swapchain images, image views, and other dependent resources.
 *
 * @param context Pointer to the Vulkan context.
 * @param swapchain Pointer to the swapchain to destroy.
 */
void vulkan_swapchain_destroy(
    vulkan_context* context,
    vulkan_swapchain* swapchain);

/**
 * @brief Acquires the index of the next available swapchain image.
 *
 * Signals the provided semaphore and/or fence when the image becomes available.
 *
 * @param context Pointer to the Vulkan context.
 * @param swapchain Pointer to the swapchain.
 * @param timeout_ns Timeout in nanoseconds to wait for an image.
 * @param image_available_semaphore Semaphore to signal when the image is ready.
 * @param fence Fence to signal when the image is ready (optional).
 * @param out_image_index Pointer that receives the acquired image index.
 *
 * @return VkResult indicating success or failure.
 */
VkResult vulkan_swapchain_acquire_next_image_index(
    vulkan_context* context,
    vulkan_swapchain* swapchain,
    u64 timeout_ns,
    VkSemaphore image_available_semaphore,
    VkFence fence,
    u32* out_image_index);

/**
 * @brief Presents a rendered image to the swapchain surface.
 *
 * Waits on the provided semaphore before presenting the specified image.
 *
 * @param context Pointer to the Vulkan context.
 * @param swapchain Pointer to the swapchain.
 * @param present_queue The queue used for presentation.
 * @param render_complete_semaphore Semaphore that signals rendering completion.
 * @param present_image_index Index of the swapchain image to present.
 *
 * @return VkResult indicating success or failure.
 */
VkResult vulkan_swapchain_present(
    vulkan_context* context,
    vulkan_swapchain* swapchain,
    VkQueue present_queue,
    VkSemaphore render_complete_semaphore,
    u32 present_image_index);