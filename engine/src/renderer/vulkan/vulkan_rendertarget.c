#include "defines.h"
#include "vulkan_rendertarget.h"

#include "utils/darray.h"

b8 vulkan_rendertarget_create(
    box_renderer_backend* backend,
    box_rendertarget* rendertarget) {
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    
    rendertarget->internal_data = ballocate(sizeof(internal_vulkan_rendertarget), MEMORY_TAG_RENDERER);
    internal_vulkan_rendertarget* internal_rendertarget = (internal_vulkan_rendertarget*)rendertarget->internal_data;
    
    // Main subpass
    VkSubpassDescription subpass = {};
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

    CHECK_VKRESULT(
        vkCreateRenderPass(
            context->device.logical_device,
            &render_pass_create_info,
            context->allocator,
            &internal_rendertarget->handle),
        "Failed to create internal Vulkan renderpass");
    
	internal_rendertarget->framebuffers = darray_reserve(vulkan_framebuffer, context->swapchain.image_count, MEMORY_TAG_RENDERER);

    // Create framebuffers.
	for (u32 i = 0; i < context->swapchain.image_count; ++i) {
        vulkan_framebuffer* framebuffer = &internal_rendertarget->framebuffers[i];

		// TODO: make this dynamic based on the currently configured attachments
		VkImageView attachments[] = { context->swapchain.views[i] };
        framebuffer->attachment_count = BX_ARRAYSIZE(attachments);

        framebuffer->attachments = ballocate(sizeof(VkImageView) * framebuffer->attachment_count, MEMORY_TAG_RENDERER);
        bcopy_memory(framebuffer->attachments, attachments, sizeof(VkImageView) * framebuffer->attachment_count);

        // Creation info
        VkFramebufferCreateInfo framebuffer_create_info = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        framebuffer_create_info.renderPass = internal_rendertarget->handle;
        framebuffer_create_info.attachmentCount = framebuffer->attachment_count;
        framebuffer_create_info.pAttachments = framebuffer->attachments;
        framebuffer_create_info.width = (u32)rendertarget->size.width;
        framebuffer_create_info.height = (u32)rendertarget->size.height;
        framebuffer_create_info.layers = 1;

        CHECK_VKRESULT(
            vkCreateFramebuffer(
                context->device.logical_device,
                &framebuffer_create_info,
                context->allocator,
                &framebuffer->handle),
            "Failed to create internal Vulkan framebuffer");
    }
    
    darray_length_set(internal_rendertarget->framebuffers, context->swapchain.image_count);
    return TRUE;
}

void vulkan_rendertarget_begin(
    box_renderer_backend* backend, 
    vulkan_command_buffer* command_buffer, 
    box_rendertarget* rendertarget, 
    b8 set_viewport, b8 set_scissor) {
    vulkan_context* context = (vulkan_context*)backend->internal_context;

    internal_vulkan_rendertarget* internal_rendertarget = (internal_vulkan_rendertarget*)rendertarget->internal_data;

    if (set_viewport) {
        // TODO: Take account for render target offset.
		VkViewport viewport = {};
        viewport.x = 0.0f;
		viewport.y = (f32)rendertarget->size.height;
		viewport.width = (f32)rendertarget->size.width;
		viewport.height = -(f32)rendertarget->size.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(command_buffer->handle, 0, 1, &viewport);
    }

    if (set_scissor) {
        // TODO: Take account for render target offset.
        VkRect2D scissor = {};
		scissor.offset.x = 0;
        scissor.offset.y = 0;
		scissor.extent.width = rendertarget->size.width;
		scissor.extent.height = rendertarget->size.height;
		vkCmdSetScissor(command_buffer->handle, 0, 1, &scissor);
    }

    VkRenderPassBeginInfo begin_info = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    begin_info.renderPass = internal_rendertarget->handle;
    begin_info.framebuffer = internal_rendertarget->framebuffers[context->image_index].handle;
    begin_info.renderArea.offset.x = rendertarget->origin.x;
    begin_info.renderArea.offset.y = rendertarget->origin.y;
    begin_info.renderArea.extent.width = rendertarget->size.width;
    begin_info.renderArea.extent.height = rendertarget->size.height;

    VkClearValue clear_values[2];
    bzero_memory(clear_values, sizeof(VkClearValue) * 2);
    clear_values[0].color.float32[0] = ((rendertarget->clear_colour >> 24) & 0xFF) / 255.0f;
    clear_values[0].color.float32[1] = ((rendertarget->clear_colour >> 16) & 0xFF) / 255.0f;
    clear_values[0].color.float32[2] = ((rendertarget->clear_colour >> 8)  & 0xFF) / 255.0f;
    clear_values[0].color.float32[3] = ((rendertarget->clear_colour)       & 0xFF) / 255.0f;

    begin_info.clearValueCount = 2;
    begin_info.pClearValues = clear_values;

    vkCmdBeginRenderPass(command_buffer->handle, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
    internal_rendertarget->state = RENDER_PASS_STATE_RECORDING;
    command_buffer->state = COMMAND_BUFFER_STATE_IN_RENDER_PASS;
}

void vulkan_rendertarget_end(
    box_renderer_backend* backend, 
    vulkan_command_buffer* command_buffer, 
    box_rendertarget* rendertarget) {
    internal_vulkan_rendertarget* internal_rendertarget = (internal_vulkan_rendertarget*)rendertarget->internal_data;

    vkCmdEndRenderPass(command_buffer->handle);
    internal_rendertarget->state = RENDER_PASS_STATE_RECORDING_ENDED;
    command_buffer->state = COMMAND_BUFFER_STATE_RECORDING;
}

void vulkan_rendertarget_destroy(
    box_renderer_backend* backend, 
    box_rendertarget* rendertarget) {
    vulkan_context* context = (vulkan_context*)backend->internal_context;

    internal_vulkan_rendertarget* internal_rendertarget = (internal_vulkan_rendertarget*)rendertarget->internal_data;

    for (u32 i = 0; i < darray_length(internal_rendertarget->framebuffers); ++i) {
        vulkan_framebuffer* framebuffer = &internal_rendertarget->framebuffers[i];

        vkDestroyFramebuffer(context->device.logical_device, framebuffer->handle, context->allocator);
        bfree(framebuffer->attachments, sizeof(VkImageView) * framebuffer->attachment_count, MEMORY_TAG_RENDERER);
    }

    darray_destroy(internal_rendertarget->framebuffers);

    vkDestroyRenderPass(context->device.logical_device, internal_rendertarget->handle, context->allocator);
    bfree(internal_rendertarget, sizeof(internal_vulkan_rendertarget), MEMORY_TAG_RENDERER);
}