#include "defines.h"
#include "vulkan_framebuffer.h"

VkResult vulkan_framebuffer_create(
    vulkan_context* context,
    vulkan_renderpass* renderpass,
    vec2 size,
    u32 attachment_count,
    VkImageView* attachments,
    vulkan_framebuffer* out_framebuffer) {
    // Take a copy of the attachments, renderpass and attachment count
    out_framebuffer->attachments = ballocate(sizeof(VkImageView) * attachment_count, MEMORY_TAG_RENDERER);
    for (u32 i = 0; i < attachment_count; ++i) {
        out_framebuffer->attachments[i] = attachments[i];
    }

    out_framebuffer->renderpass = renderpass;
    out_framebuffer->attachment_count = attachment_count;

    // Creation info
    VkFramebufferCreateInfo framebuffer_create_info = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    framebuffer_create_info.renderPass = renderpass->handle;
    framebuffer_create_info.attachmentCount = attachment_count;
    framebuffer_create_info.pAttachments = out_framebuffer->attachments;
    framebuffer_create_info.width = (u32)size.width;
    framebuffer_create_info.height = (u32)size.height;
    framebuffer_create_info.layers = 1;

    return vkCreateFramebuffer(
        context->device.logical_device,
        &framebuffer_create_info,
        context->allocator,
        &out_framebuffer->handle);
}

void vulkan_framebuffer_destroy(vulkan_context* context, vulkan_framebuffer* framebuffer) {
    if (framebuffer && framebuffer->handle) {
        vkDestroyFramebuffer(context->device.logical_device, framebuffer->handle, context->allocator);

        if (framebuffer->attachments) {
            bfree(framebuffer->attachments, sizeof(VkImageView) * framebuffer->attachment_count, MEMORY_TAG_RENDERER);
            framebuffer->attachments = 0;
        }

        framebuffer->handle = 0;
        framebuffer->attachment_count = 0;
        framebuffer->renderpass = 0;
    }
}