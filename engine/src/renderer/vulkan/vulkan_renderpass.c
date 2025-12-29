#include "defines.h"
#include "vulkan_renderpass.h"

VkResult vulkan_renderpass_create(
    vulkan_context* context,
    vulkan_renderpass* out_renderpass,
    vec2 origin, vec2 size) {
    out_renderpass->origin = origin;
    out_renderpass->size = size;
    out_renderpass->clear_colour = 0x191919FF;

    // Main subpass
    VkSubpassDescription subpass = {0};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    // Attachments TODO: make this configurable.

    // Color attachment
    VkAttachmentDescription color_attachment;
    color_attachment.format = context->swapchain.image_format.format;  // TODO: configurable
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;      // Do not expect any particular layout before render pass starts.
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  // Transitioned to after the render pass
    color_attachment.flags = 0;

    VkAttachmentReference color_attachment_reference;
    color_attachment_reference.attachment = 0;  // Attachment description array index
    color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_reference;

    // TODO: other attachment types (input, resolve, preserve)

    // Input from a shader
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = 0;

    // Attachments used for multisampling colour attachments
    subpass.pResolveAttachments = 0;

    // Attachments not used in this subpass, but must be preserved for the next.
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = 0;

    // Render pass dependencies. TODO: make this configurable.
    VkSubpassDependency dependency;
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dependencyFlags = 0;

    // Render pass create.
    VkRenderPassCreateInfo render_pass_create_info = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    render_pass_create_info.attachmentCount = 1;
    render_pass_create_info.pAttachments = &color_attachment;
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &subpass;
    render_pass_create_info.dependencyCount = 1;
    render_pass_create_info.pDependencies = &dependency;
    render_pass_create_info.pNext = 0;
    render_pass_create_info.flags = 0;

    VkResult result = vkCreateRenderPass(context->device.logical_device, &render_pass_create_info, context->allocator, &out_renderpass->handle);
    if (!vulkan_result_is_success(result)) return result;

    out_renderpass->state = RENDER_PASS_STATE_READY;
    return VK_SUCCESS;
}

void vulkan_renderpass_destroy(vulkan_context* context, vulkan_renderpass* renderpass) {
    if (renderpass && renderpass->handle) {
        vkDestroyRenderPass(context->device.logical_device, renderpass->handle, context->allocator);
        renderpass->state = RENDER_PASS_STATE_NOT_ALLOCATED;
        renderpass->handle = 0;
    }
}

void vulkan_renderpass_begin(
    vulkan_command_buffer* command_buffer,
    vulkan_renderpass* renderpass,
    vulkan_framebuffer* frame_buffer) {
    VkRenderPassBeginInfo begin_info = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    begin_info.renderPass = renderpass->handle;
    begin_info.framebuffer = frame_buffer->handle;
    begin_info.renderArea.offset.x = renderpass->origin.x;
    begin_info.renderArea.offset.y = renderpass->origin.y;
    begin_info.renderArea.extent.width = renderpass->size.width;
    begin_info.renderArea.extent.height = renderpass->size.height;

    VkClearValue clear_values[2];
    bzero_memory(clear_values, sizeof(VkClearValue) * 2);
    clear_values[0].color.float32[0] = ((renderpass->clear_colour >> 24) & 0xFF) / 255.0f;
    clear_values[0].color.float32[1] = ((renderpass->clear_colour >> 16) & 0xFF) / 255.0f;
    clear_values[0].color.float32[2] = ((renderpass->clear_colour >> 8)  & 0xFF) / 255.0f;
    clear_values[0].color.float32[3] = ((renderpass->clear_colour)       & 0xFF) / 255.0f;

    begin_info.clearValueCount = 2;
    begin_info.pClearValues = clear_values;

    vkCmdBeginRenderPass(command_buffer->handle, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
    renderpass->state = RENDER_PASS_STATE_RECORDING;
    command_buffer->state = COMMAND_BUFFER_STATE_IN_RENDER_PASS;
}

void vulkan_renderpass_end(vulkan_command_buffer* command_buffer, vulkan_renderpass* renderpass) {
    vkCmdEndRenderPass(command_buffer->handle);
    renderpass->state = RENDER_PASS_STATE_RECORDING_ENDED;
    command_buffer->state = COMMAND_BUFFER_STATE_RECORDING;
}