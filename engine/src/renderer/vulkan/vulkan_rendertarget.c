#include "defines.h"
#include "vulkan_rendertarget.h"

#include "utils/darray.h"

#include "vulkan_image.h"

b8 vulkan_rendertarget_create(
    box_renderer_backend* backend,
    box_rendertarget* out_rendertarget) {
    vulkan_context* context = (vulkan_context*)backend->internal_context;

    vulkan_image* images = darray_reserve(vulkan_image, out_rendertarget->attachment_count * context->config.frames_in_flight, MEMORY_TAG_RENDERER);
    for (u32 i = 0; i < out_rendertarget->attachment_count * context->config.frames_in_flight; ++i) {
        u32 frame = i / context->config.frames_in_flight;
        u32 attachment = i % context->config.frames_in_flight;

        vulkan_image* image = darray_push_empty(images);

        CHECK_VKRESULT(
            vulkan_image_create(
                context, 
                out_rendertarget->size,
                box_format_to_vulkan_type(out_rendertarget->attachments[attachment].format), 
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
                TRUE, 
                VK_IMAGE_ASPECT_COLOR_BIT, 
                image),
            "Failed to create internal Vulkan rendertarget image");
    }

    if (!vulkan_rendertarget_create_external(context, images, darray_length(images), out_rendertarget))
        return FALSE;

    darray_destroy(images);
    return TRUE;
}

b8 vulkan_rendertarget_create_external(
    vulkan_context* context, 
    vulkan_image* external_images, 
    u32 image_count, 
    box_rendertarget* out_rendertarget) {
    out_rendertarget->internal_data = ballocate(sizeof(internal_vulkan_rendertarget), MEMORY_TAG_RENDERER);
    internal_vulkan_rendertarget* internal_rendertarget = (internal_vulkan_rendertarget*)out_rendertarget->internal_data;

    u64 total_size = sizeof(vulkan_image) * out_rendertarget->attachment_count * context->config.frames_in_flight;

    internal_rendertarget->attachments = ballocate(total_size, MEMORY_TAG_RENDERER);
    bcopy_memory(internal_rendertarget->attachments, external_images, total_size);

	internal_rendertarget->framebuffers = darray_reserve(VkFramebuffer, context->config.frames_in_flight, MEMORY_TAG_RENDERER);
    
    VkAttachmentReference* colour_attachments = NULL;
    VkAttachmentDescription* attachments = darray_reserve(VkAttachmentDescription, out_rendertarget->attachment_count, MEMORY_TAG_RENDERER);

    // Main subpass
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    
    for (u32 i = 0; i < out_rendertarget->attachment_count; ++i) {
        const box_rendertarget_attachment* attachment = &out_rendertarget->attachments[i];

        VkAttachmentDescription* attachment_desc = darray_push_empty(attachments);
        attachment_desc->format = box_format_to_vulkan_type(attachment->format);
        attachment_desc->samples = VK_SAMPLE_COUNT_1_BIT;
        attachment_desc->loadOp = box_load_op_to_vulkan_type(attachment->load_op);
        attachment_desc->storeOp = box_store_op_to_vulkan_type(attachment->store_op);
        attachment_desc->stencilLoadOp = box_load_op_to_vulkan_type(attachment->stencil_load_op);
        attachment_desc->stencilStoreOp = box_store_op_to_vulkan_type(attachment->stencil_store_op);
        attachment_desc->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment_desc->finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        switch (attachment->type) {
            case BOX_ATTACHMENT_COLOR:
            //case BOX_ATTACHMENT_DEPTH: 
            //case BOX_ATTACHMENT_STENCIL:
            //case BOX_ATTACHMENT_DEPTH_STENCIL:
                if (!colour_attachments)
                    colour_attachments = darray_create(VkAttachmentReference, MEMORY_TAG_RENDERER);

                VkAttachmentReference* attachment_reference = darray_push_empty(colour_attachments);
                attachment_reference->attachment = i;
                attachment_reference->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                break;

            default:
                BX_ASSERT(FALSE && "Unsupported attachment type");
                continue;
        }
    }

    subpass.colorAttachmentCount = colour_attachments ? darray_length(colour_attachments) : 0;
    subpass.pColorAttachments = colour_attachments;

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
    render_pass_create_info.attachmentCount = darray_length(attachments);
    render_pass_create_info.pAttachments = attachments;
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

    darray_destroy(attachments);
    if (colour_attachments != NULL) darray_destroy(colour_attachments);

    for (u32 i = 0; i < context->config.frames_in_flight; ++i) {
        VkImageView* views = darray_reserve(VkImageView, out_rendertarget->attachment_count, MEMORY_TAG_RENDERER);
        for (u32 j = 0; j < out_rendertarget->attachment_count; ++j)
            darray_push(views, internal_rendertarget->attachments[j + i * out_rendertarget->attachment_count].view);

        VkFramebufferCreateInfo framebuffer_create_info = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        framebuffer_create_info.renderPass = internal_rendertarget->handle;
        framebuffer_create_info.attachmentCount = out_rendertarget->attachment_count;
        framebuffer_create_info.pAttachments = views;
        framebuffer_create_info.width = out_rendertarget->size.width;
        framebuffer_create_info.height = out_rendertarget->size.height;
        framebuffer_create_info.layers = 1;

        VkFramebuffer framebuffer;

        CHECK_VKRESULT(
            vkCreateFramebuffer(
                context->device.logical_device,
                &framebuffer_create_info,
                context->allocator,
                &framebuffer),
            "Failed to create internal Vulkan framebuffer");
        
        darray_push(internal_rendertarget->framebuffers, framebuffer);
        darray_destroy(views);
    }

    return TRUE;
}

void vulkan_rendertarget_begin(
    vulkan_context* context,
    vulkan_command_buffer* command_buffer, 
    box_rendertarget* rendertarget, 
    b8 set_viewport, b8 set_scissor) {
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
    begin_info.framebuffer = internal_rendertarget->framebuffers[context->current_frame];
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
    vulkan_context* context,
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

    vkDestroyRenderPass(context->device.logical_device, internal_rendertarget->handle, context->allocator);
    bfree(internal_rendertarget, sizeof(internal_vulkan_rendertarget), MEMORY_TAG_RENDERER);
}