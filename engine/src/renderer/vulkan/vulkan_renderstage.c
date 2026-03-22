#include "defines.h"
#include "vulkan_renderstage.h"

#include "utils/darray.h"

VkResult vulkan_renderstage_create_layout(
    vulkan_context* context,
    VkPipelineShaderStageCreateInfo** out_shader_stages,
    box_renderstage_layout* config,
    box_renderstage* out_renderstage) {
    internal_vulkan_renderstage* internal_renderstage = (internal_vulkan_renderstage*)out_renderstage->internal_data;
    
    for (u32 i = 0; i < BOX_SHADER_STAGE_TYPE_MAX; ++i) {
        const box_shader_src* source = &config->stages[i];
        if (source->size == 0) continue;
        BX_ASSERT(source->data != NULL || source->size == 0 && "Invalid shader stage source: size > 0 but data pointer is NULL");

		VkPipelineShaderStageCreateInfo* stage_info = darray_push_empty(*out_shader_stages);
		stage_info->sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage_info->stage  = box_shader_type_to_vulkan_type((box_shader_stage_type)i);
		stage_info->pName  = "main";

        VkShaderModuleCreateInfo create_info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		create_info.pCode    = source->data;
		create_info.codeSize = source->size;
		VkResult result = vkCreateShaderModule(context->device.logical_device, &create_info, context->allocator, &stage_info->module);
		if (!vulkan_result_is_success(result)) return result;
    }

    // Setup descriptor layout / memory for renderstage
    if (config->descriptor_count > 0) {
        // Collect descriptor data into vulkan structs.
		VkDescriptorSetLayoutBinding* descriptor_bindings = darray_reserve(VkDescriptorSetLayoutBinding, config->descriptor_count, MEMORY_TAG_RENDERER);
		VkDescriptorPoolSize* descriptor_pools = darray_reserve(VkDescriptorPoolSize, config->descriptor_count, MEMORY_TAG_RENDERER);

        for (u32 i = 0; i < config->descriptor_count; ++i) {
			VkDescriptorSetLayoutBinding* binding = darray_push_empty(descriptor_bindings);
			binding->binding = config->descriptors[i].binding;
			binding->descriptorType = box_descriptor_type_to_vulkan_type(config->descriptors[i].descriptor_type);
			binding->stageFlags = box_shader_type_to_vulkan_type(config->descriptors[i].stage_type);
			binding->descriptorCount = 1;

			VkDescriptorPoolSize* pool_size = darray_push_empty(descriptor_pools);
			pool_size->descriptorCount = context->config.frames_in_flight;
			pool_size->type = binding->descriptorType;
		}
        // ------------------------------------------

        // Create descriptor set layout.
		VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		layoutInfo.bindingCount = darray_length(descriptor_bindings);
		layoutInfo.pBindings = descriptor_bindings;
		VkResult result = vkCreateDescriptorSetLayout(context->device.logical_device, &layoutInfo, context->allocator, &internal_renderstage->descriptor);
		if (!vulkan_result_is_success(result)) return result;
        // ------------------------------------------

        // Create descriptor pool.
		VkDescriptorPoolCreateInfo pool_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		pool_info.poolSizeCount = darray_length(descriptor_pools);
		pool_info.pPoolSizes = descriptor_pools;
		pool_info.maxSets = context->config.frames_in_flight;

		result = vkCreateDescriptorPool(context->device.logical_device, &pool_info, context->allocator, &internal_renderstage->descriptor_pool);
		if (!vulkan_result_is_success(result)) return result;
        // ------------------------------------------

        // Create descriptor sets per frame in flight.
		internal_renderstage->descriptor_sets = darray_reserve(VkDescriptorSet, context->config.frames_in_flight, MEMORY_TAG_RENDERER);

        VkDescriptorSetLayout* layouts = darray_reserve(VkDescriptorSetLayout, context->config.frames_in_flight, MEMORY_TAG_RENDERER);
		for (u32 i = 0; i < context->config.frames_in_flight; ++i)
			darray_push(layouts, internal_renderstage->descriptor);

        VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		alloc_info.descriptorPool = internal_renderstage->descriptor_pool;
		alloc_info.descriptorSetCount = darray_length(layouts);
		alloc_info.pSetLayouts = layouts;
		result = vkAllocateDescriptorSets(context->device.logical_device, &alloc_info, internal_renderstage->descriptor_sets);
		if (!vulkan_result_is_success(result)) return result;

		darray_length_set(internal_renderstage->descriptor_sets, alloc_info.descriptorSetCount);
        // ------------------------------------------

        // Destory all temp memory...
		darray_destroy(descriptor_pools);
		darray_destroy(descriptor_bindings);
		darray_destroy(layouts);
    }

    VkPipelineLayoutCreateInfo create_info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	if (config->descriptor_count > 0) {
		create_info.setLayoutCount = 1;
		create_info.pSetLayouts = &internal_renderstage->descriptor;
	}

    return vkCreatePipelineLayout(
        context->device.logical_device,
        &create_info,
        context->allocator,
        &internal_renderstage->layout);
}

b8 vulkan_renderstage_create_graphic(
    box_renderer_backend* backend,
    box_graphicstage_config* config, 
    box_rendertarget* bound_rendertarget, 
    box_renderstage* out_renderstage) {
	BX_ASSERT(backend != NULL && config != NULL && bound_rendertarget != NULL && out_renderstage != NULL && "Invalid arguments passed to vulkan_renderstage_create_graphic");
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    
    out_renderstage->internal_data = ballocate(sizeof(internal_vulkan_renderstage), MEMORY_TAG_RENDERER);
    internal_vulkan_renderstage* internal_renderstage = (internal_vulkan_renderstage*)out_renderstage->internal_data;

    out_renderstage->pipeline_type = RENDERER_MODE_GRAPHICS;
    out_renderstage->descriptors = darray_from_data(box_descriptor_desc, config->layout.descriptor_count, config->layout.descriptors, MEMORY_TAG_RENDERER);
    internal_renderstage->graphics.vertex_buffer = config->vertex_buffer;
    internal_renderstage->graphics.index_buffer = config->index_buffer;

    VkPipelineShaderStageCreateInfo* shader_stages = darray_create(VkPipelineShaderStageCreateInfo, MEMORY_TAG_RENDERER);

    CHECK_VKRESULT(
        vulkan_renderstage_create_layout(
            context, 
            &shader_stages,
            &config->layout, 
            out_renderstage), 
        "Failed to create internal Vulkan pipeline layout");

    VkGraphicsPipelineCreateInfo pipeline_create_info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pipeline_create_info.stageCount = darray_length(shader_stages);
    pipeline_create_info.pStages = shader_stages;

    // Attach pipeline to the render pass.
    internal_vulkan_rendertarget* vulkan_rendertarget = (internal_vulkan_rendertarget*)bound_rendertarget->internal_data;
    pipeline_create_info.renderPass = vulkan_rendertarget->handle;
    pipeline_create_info.layout = internal_renderstage->layout;

    // Recieve viewport/scissor from renderpass
    VkViewport viewport = {};
    viewport.x = (f32)bound_rendertarget->origin.x;
    viewport.y = (f32)(bound_rendertarget->origin.y + bound_rendertarget->size.height);
    viewport.width  = (f32)bound_rendertarget->size.width;
    viewport.height = -(f32)bound_rendertarget->size.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    // Scissor
    VkRect2D scissor = {};
    scissor.offset.x = bound_rendertarget->origin.x;
    scissor.offset.y = bound_rendertarget->origin.y;
    scissor.extent.width  = bound_rendertarget->size.width;
    scissor.extent.height = bound_rendertarget->size.height;

    // Vertex input configuration
    // Calculate total vertex stride and fill attribute descriptions.   
    VkVertexInputBindingDescription binding_desc = {};
    VkVertexInputAttributeDescription* attributes = darray_reserve(VkVertexInputAttributeDescription, config->vertex_attribute_count, MEMORY_TAG_RENDERER);

    u64 attribute_stride = 0;
    for (u32 i = 0; i < config->vertex_attribute_count; ++i) {
        box_render_format attribute = config->vertex_attributes[i];

        VkVertexInputAttributeDescription* descriptor = darray_push_empty(attributes);
        descriptor->binding = 0;
        descriptor->location = i;
        descriptor->format = box_render_format_to_vulkan_type(attribute);
        descriptor->offset = attribute_stride;
        attribute_stride += box_render_format_size(attribute);
    }

    binding_desc.binding = 0;
    binding_desc.stride = attribute_stride;
    binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    // Colour attachments 
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};

    // TODO: Enable / disable blending.
    colorBlendAttachment.blendEnable = VK_FALSE;

    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    // Viewport state
    VkPipelineViewportStateCreateInfo viewport_state = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewport_state.viewportCount = 1;
    viewport_state.pViewports    = &viewport;
    viewport_state.scissorCount  = 1;
    viewport_state.pScissors     = &scissor;

    // Rasterization state
    VkPipelineRasterizationStateCreateInfo rasterizer = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode                = VK_CULL_MODE_NONE;
    rasterizer.frontFace               = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable         = VK_FALSE;

    // Multisampling state
    VkPipelineMultisampleStateCreateInfo multisampling = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisampling.sampleShadingEnable  = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // No MSAA

    // Depth / stencil state
    VkPipelineDepthStencilStateCreateInfo depth_stencil = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    depth_stencil.depthTestEnable       = VK_FALSE; // TODO: Make configurable
    depth_stencil.depthWriteEnable      = VK_FALSE;
    depth_stencil.depthCompareOp        = VK_COMPARE_OP_LESS;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.stencilTestEnable     = VK_FALSE;

    // Color blending state
    VkPipelineColorBlendStateCreateInfo color_blending = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    color_blending.logicOpEnable   = VK_FALSE;
    color_blending.logicOp         = VK_LOGIC_OP_COPY;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments    = &colorBlendAttachment;

    // TODO: Take a closer look at this...
    VkPipelineDynamicStateCreateInfo dynamic_state = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamic_state.dynamicStateCount = 0;
    dynamic_state.pDynamicStates    = NULL;

    // Vertex input state
    VkPipelineVertexInputStateCreateInfo vertex_input_state = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

    // Only configure if vertex attributes exist
    if (config->vertex_attribute_count > 0) {
        vertex_input_state.vertexAttributeDescriptionCount = darray_length(attributes);
        vertex_input_state.pVertexAttributeDescriptions = attributes;
        vertex_input_state.vertexBindingDescriptionCount = 1;
        vertex_input_state.pVertexBindingDescriptions = &binding_desc;
    }

    // Input assembly state
    VkPipelineInputAssemblyStateCreateInfo input_assembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // TODO: Use config->topology_type for this
    input_assembly.primitiveRestartEnable = VK_FALSE;

    // Attach all configured states to the pipeline create info
    pipeline_create_info.pViewportState = &viewport_state;
    pipeline_create_info.pRasterizationState = &rasterizer;
    pipeline_create_info.pMultisampleState = &multisampling;
    pipeline_create_info.pDepthStencilState = &depth_stencil;
    pipeline_create_info.pColorBlendState = &color_blending;
    pipeline_create_info.pDynamicState = &dynamic_state;
    pipeline_create_info.pVertexInputState = &vertex_input_state;
    pipeline_create_info.pInputAssemblyState = &input_assembly;

    CHECK_VKRESULT(
        vkCreateGraphicsPipelines(
            context->device.logical_device, 
            VK_NULL_HANDLE, 
            1, 
            &pipeline_create_info, 
            context->allocator, 
            &internal_renderstage->handle),
        "Failed to create internal Vulkan pipeline");
    
    darray_destroy(attributes);

    for (u32 i = 0; i < darray_length(shader_stages); ++i)
		vkDestroyShaderModule(context->device.logical_device, shader_stages[i].module, context->allocator);
    darray_destroy(shader_stages);
    return TRUE;
}

b8 vulkan_renderstage_create_compute(
    box_renderer_backend* backend, 
    box_computestage_config* config, 
    box_renderstage* out_renderstage) {
	BX_ASSERT(backend != NULL && config != NULL && out_renderstage != NULL && "Invalid arguments passed to vulkan_renderstage_create_compute");
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    
    out_renderstage->internal_data = ballocate(sizeof(internal_vulkan_renderstage), MEMORY_TAG_RENDERER);
    internal_vulkan_renderstage* internal_renderstage = (internal_vulkan_renderstage*)out_renderstage->internal_data;

    out_renderstage->pipeline_type = RENDERER_MODE_COMPUTE;
    out_renderstage->descriptors = darray_from_data(box_descriptor_desc, config->layout.descriptor_count, config->layout.descriptors, MEMORY_TAG_RENDERER);
    
    VkPipelineShaderStageCreateInfo* shader_stages = darray_create(VkPipelineShaderStageCreateInfo, MEMORY_TAG_RENDERER);

    CHECK_VKRESULT(
        vulkan_renderstage_create_layout(
            context, 
            &shader_stages,
            &config->layout, 
            out_renderstage), 
        "Failed to create internal Vulkan pipeline layout");
    
#if BOX_ENABLE_VALIDATION
    if (darray_length(shader_stages) > 1) {
        BX_ERROR("vulkan_renderstage_create_compute(): Compute renderstage contain more than 1 stage (only needs a single compute shader stage)");
        return FALSE;
    }

    if (shader_stages[0].stage != VK_SHADER_STAGE_COMPUTE_BIT) {
        BX_ERROR("vulkan_renderstage_create_compute(): Compute renderstage contains no compute stages");
        return FALSE;
    }
#endif

    VkComputePipelineCreateInfo pipeline_create_info = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    pipeline_create_info.stage = shader_stages[0];
    pipeline_create_info.layout = internal_renderstage->layout;

    CHECK_VKRESULT(
        vkCreateComputePipelines(
            context->device.logical_device, 
            VK_NULL_HANDLE, 
            1, 
            &pipeline_create_info, 
            context->allocator, 
            &internal_renderstage->handle),
        "Failed to create internal Vulkan pipeline");

    for (u32 i = 0; i < darray_length(shader_stages); ++i)
		vkDestroyShaderModule(context->device.logical_device, shader_stages[i].module, context->allocator);
    darray_destroy(shader_stages);
    return TRUE;
}

b8 vulkan_renderstage_update_descriptors(
    box_renderer_backend* backend,
    box_update_descriptors* descriptors,
    u32 descriptor_count) {
    BX_ASSERT(backend != NULL && descriptors && "Invalid arguments passed to vulkan_renderstage_update_descriptors");
    vulkan_context* context = (vulkan_context*)backend->internal_context;

    u32 image_count = context->config.frames_in_flight;
    u32 max_writes  = descriptor_count * image_count;

    VkWriteDescriptorSet* write_commands = darray_reserve(VkWriteDescriptorSet, max_writes, MEMORY_TAG_RENDERER);
    VkDescriptorBufferInfo* buffer_infos = darray_reserve(VkDescriptorBufferInfo, max_writes, MEMORY_TAG_RENDERER);
    VkDescriptorImageInfo* image_infos = darray_reserve(VkDescriptorImageInfo, max_writes, MEMORY_TAG_RENDERER);

    for (u32 i = 0; i < descriptor_count; ++i) {
        box_update_descriptors* write = &descriptors[i];
        const box_descriptor_desc* layout_desc = &write->renderstage->descriptors[write->binding];

        if (write->type != layout_desc->descriptor_type) {
            BX_ERROR("Descriptor type mismatch at binding %u (write=%u, layout=%u)", write->binding, write->type, layout_desc->descriptor_type);
            continue;
        }

        BX_ASSERT(write->renderstage != NULL && "Malformed data when updating descriptors");
        internal_vulkan_renderstage* internal_renderstage = (internal_vulkan_renderstage*)write->renderstage->internal_data;

        for (u32 j = 0; j < image_count; ++j) {
            VkWriteDescriptorSet* descriptor_write = darray_push_empty(write_commands);
			descriptor_write->sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptor_write->dstSet          = internal_renderstage->descriptor_sets[j];
			descriptor_write->dstBinding      = write->binding;
			descriptor_write->dstArrayElement = 0;
			descriptor_write->descriptorType  = box_descriptor_type_to_vulkan_type(write->type);
			descriptor_write->descriptorCount = 1;

            switch (write->type) {
                case BOX_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                    internal_vulkan_renderbuffer* renderbuffer = (internal_vulkan_renderbuffer*)write->buffer->internal_data;

                    VkDescriptorBufferInfo* buffer_info = darray_push_empty(buffer_infos);
                    buffer_info->buffer = renderbuffer->handle;
                    buffer_info->range  = write->buffer->buffer_size;
                    buffer_info->offset = 0;
                    descriptor_write->pBufferInfo = buffer_info;
					break;

                case BOX_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                case BOX_DESCRIPTOR_TYPE_IMAGE_SAMPLER:
                    internal_vulkan_texture* texture = (internal_vulkan_texture*)write->texture->internal_data;

                    VkDescriptorImageInfo* image_info = darray_push_empty(image_infos);
                    image_info->sampler     = texture->sampler;
                    image_info->imageView   = texture->image.view;
                    image_info->imageLayout = texture->image.layout;
                    descriptor_write->pImageInfo = image_info;
					break;

                default:
                    BX_ASSERT(FALSE && "Unsupported descriptor type");
                    continue;
            }
        }
    }

    if (darray_length(write_commands) > 0)
        vkUpdateDescriptorSets(context->device.logical_device, darray_length(write_commands), write_commands, 0, NULL);

    darray_destroy(buffer_infos);
    darray_destroy(image_infos);
    darray_destroy(write_commands);

    return TRUE;
}

void vulkan_renderstage_bind(
    vulkan_context* context, 
    vulkan_command_buffer* command_buffer, 
    box_renderstage* renderstage) {
    internal_vulkan_renderstage* internal_renderstage = (internal_vulkan_renderstage*)renderstage->internal_data;

    VkPipelineBindPoint bind_point = 0;
    switch (renderstage->pipeline_type) {
        case RENDERER_MODE_GRAPHICS: bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS; break;
        case RENDERER_MODE_COMPUTE:  bind_point = VK_PIPELINE_BIND_POINT_COMPUTE; break;

        default: 
            BX_ASSERT(FALSE && "Unsupported renderstage type");
            break;
    }

	vkCmdBindPipeline(command_buffer->handle, bind_point, internal_renderstage->handle);

	if (internal_renderstage->descriptor_sets)
		vkCmdBindDescriptorSets(command_buffer->handle, bind_point, internal_renderstage->layout, 0, 1, &internal_renderstage->descriptor_sets[context->current_frame], 0, 0);

    switch (renderstage->pipeline_type) {
        case RENDERER_MODE_GRAPHICS:
            if (!internal_renderstage->graphics.vertex_buffer) break;
            VkDeviceSize offset = 0;
            
            internal_vulkan_renderbuffer* vertex_buffer = (internal_vulkan_renderbuffer*)internal_renderstage->graphics.vertex_buffer->internal_data;
            vkCmdBindVertexBuffers(command_buffer->handle, 0, 1, &vertex_buffer->handle, &offset);

            if (internal_renderstage->graphics.index_buffer != NULL) {
                internal_vulkan_renderbuffer* index_buffer = (internal_vulkan_renderbuffer*)internal_renderstage->graphics.index_buffer->internal_data;
                vkCmdBindIndexBuffer(command_buffer->handle, index_buffer->handle, offset, VK_INDEX_TYPE_UINT16); // TODO: Customize index type?
            }
            break;
    }
}

void vulkan_renderstage_destroy(
    box_renderer_backend* backend, 
    box_renderstage* renderstage) {
    BX_ASSERT(backend != NULL && renderstage != NULL && "Invalid arguments passed to vulkan_renderstage_destroy");
    vulkan_context* context = (vulkan_context*)backend->internal_context;
	if (context->device.logical_device) vkDeviceWaitIdle(context->device.logical_device);

    internal_vulkan_renderstage* internal_renderstage = (internal_vulkan_renderstage*)renderstage->internal_data;

    if (internal_renderstage != NULL) {
        if (internal_renderstage->descriptor_sets) darray_destroy(internal_renderstage->descriptor_sets);
        
        if (internal_renderstage->descriptor)
            vkDestroyDescriptorSetLayout(context->device.logical_device, internal_renderstage->descriptor, context->allocator);
        
        if (internal_renderstage->descriptor_pool)
            vkDestroyDescriptorPool(context->device.logical_device, internal_renderstage->descriptor_pool, context->allocator);

        if (internal_renderstage->layout)
            vkDestroyPipelineLayout(context->device.logical_device, internal_renderstage->layout, context->allocator);

        if (internal_renderstage->handle)
            vkDestroyPipeline(context->device.logical_device, internal_renderstage->handle, context->allocator);

        bfree(renderstage->internal_data, sizeof(internal_vulkan_renderstage), MEMORY_TAG_RENDERER);
    }
}