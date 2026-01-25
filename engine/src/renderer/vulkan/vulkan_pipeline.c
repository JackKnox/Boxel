#include "defines.h"
#include "vulkan_pipeline.h"

#include "utils/darray.h"

VkResult create_pipeline_layout(vulkan_context* context, box_renderstage* shader, vulkan_pipeline* out_pipeline) {
	if (shader->layout.descriptor_count > 0) {
		VkDescriptorSetLayoutBinding* descriptor_bindings = darray_reserve(VkDescriptorSetLayoutBinding, shader->layout.descriptor_count, MEMORY_TAG_RENDERER);
		VkDescriptorPoolSize* descriptor_pools = darray_reserve(VkDescriptorPoolSize, shader->layout.descriptor_count, MEMORY_TAG_RENDERER);

		for (u32 i = 0; i < shader->layout.descriptor_count; ++i) {
			VkDescriptorSetLayoutBinding* binding = darray_push_empty(descriptor_bindings);
			binding->binding = i;
			binding->descriptorCount = 1;
			binding->descriptorType = box_descriptor_type_to_vulkan_type(shader->layout.descriptors[i].descriptor_type);
			binding->stageFlags = box_shader_type_to_vulkan_type(shader->layout.descriptors[i].stage_type);

			VkDescriptorPoolSize* pool_size = darray_push_empty(descriptor_pools);
			pool_size->descriptorCount = context->swapchain.image_count;
			pool_size->type = binding->descriptorType;
		}

		VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		layoutInfo.bindingCount = darray_length(descriptor_bindings);
		layoutInfo.pBindings = descriptor_bindings;

		VkResult result = vkCreateDescriptorSetLayout(context->device.logical_device, &layoutInfo, context->allocator, &out_pipeline->descriptor);
		if (!vulkan_result_is_success(result)) return result;

		VkDescriptorPoolCreateInfo pool_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		pool_info.poolSizeCount = darray_length(descriptor_pools);
		pool_info.pPoolSizes = descriptor_pools;
		pool_info.maxSets = context->swapchain.image_count;

		result = vkCreateDescriptorPool(context->device.logical_device, &pool_info, context->allocator, &out_pipeline->descriptor_pool);
		if (!vulkan_result_is_success(result)) return result;

		darray_destroy(descriptor_pools);
		darray_destroy(descriptor_bindings);

		out_pipeline->descriptor_sets = darray_reserve(VkDescriptorSet, context->swapchain.image_count, MEMORY_TAG_RENDERER);

		VkDescriptorSetLayout* layouts = darray_reserve(VkDescriptorSetLayout, context->swapchain.image_count, MEMORY_TAG_RENDERER);
		for (u32 i = 0; i < context->swapchain.image_count; ++i)
			darray_push(layouts, out_pipeline->descriptor);
		
		VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		alloc_info.descriptorPool = out_pipeline->descriptor_pool;
		alloc_info.descriptorSetCount = darray_length(layouts);
		alloc_info.pSetLayouts = layouts;
		result = vkAllocateDescriptorSets(context->device.logical_device, &alloc_info, out_pipeline->descriptor_sets);
		if (!vulkan_result_is_success(result)) return result;

		darray_length_set(out_pipeline->descriptor_sets, alloc_info.descriptorSetCount);

		darray_destroy(layouts);
	}

	VkPipelineLayoutCreateInfo create_info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	if (shader->layout.descriptor_count > 0) {
		create_info.setLayoutCount = 1;
		create_info.pSetLayouts = &out_pipeline->descriptor;
	}

	return vkCreatePipelineLayout(
		context->device.logical_device,
		&create_info,
		context->allocator,
		&out_pipeline->layout);
}

VkResult vulkan_graphics_pipeline_create(
	vulkan_context* context, 
	vulkan_pipeline* out_pipeline, 
	vulkan_renderpass* renderpass, 
	box_renderstage* shader) {
	// Fill shader stages with data from shader array
	VkPipelineShaderStageCreateInfo* shader_stages = darray_create(VkPipelineShaderStageCreateInfo, MEMORY_TAG_RENDERER);
	for (int i = 0; i < BOX_SHADER_STAGE_TYPE_MAX; ++i) {
		shader_stage* stage = &shader->stages[i];
		if (stage->file_size == 0) {
			BX_ASSERT(stage->file_data == NULL && "Shader stage says file size is zero but contains data?");
			continue;
		}

		VkShaderModule shader_module;
		VkShaderModuleCreateInfo create_info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		create_info.pCode = stage->file_data;
		create_info.codeSize = stage->file_size;
		VkResult result = vkCreateShaderModule(context->device.logical_device, &create_info, context->allocator, &shader_module);
		if (!vulkan_result_is_success(result)) return result;

		VkPipelineShaderStageCreateInfo* stage_info = darray_push_empty(shader_stages);
		stage_info->sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage_info->stage  = box_shader_type_to_vulkan_type((box_shader_stage_type)i);
		stage_info->module = shader_module;
		stage_info->pName  = "main";

		bfree(stage->file_data, stage->file_size, MEMORY_TAG_CORE); // Comes from filesystem_read_entire_binary_file
	}

	VkPipelineViewportStateCreateInfo viewport_state = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	VkPipelineRasterizationStateCreateInfo rasterizer = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	VkPipelineMultisampleStateCreateInfo multisampling = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	VkPipelineDepthStencilStateCreateInfo depth_stencil = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	VkPipelineColorBlendStateCreateInfo color_blending = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	VkPipelineDynamicStateCreateInfo dynamic_state = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	VkPipelineVertexInputStateCreateInfo vertex_input_info = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	VkPipelineInputAssemblyStateCreateInfo input_assembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };

	// Recieve viewport/scissor from renderpass
	VkViewport viewport;
	viewport.x = 0.0f;
	viewport.y = (f32)renderpass->size.height;
	viewport.width = (f32)renderpass->size.width;
	viewport.height = -(f32)renderpass->size.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	// Scissor
	VkRect2D scissor;
	scissor.offset.x = scissor.offset.y = 0;
	scissor.extent.width = renderpass->size.width;
	scissor.extent.height = renderpass->size.height;

	// Viewport state
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	// Rasterizer
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	// Multisampling.
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// Depth and stencil testing.
	depth_stencil.depthTestEnable = VK_FALSE;
	depth_stencil.depthWriteEnable = VK_FALSE;
	depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depth_stencil.depthBoundsTestEnable = VK_FALSE;
	depth_stencil.stencilTestEnable = VK_FALSE;

	// Colour attachments 
	VkPipelineColorBlendAttachmentState colorBlendAttachment = { 0 };
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	// Colour blending settings
	color_blending.logicOpEnable = VK_FALSE;
	color_blending.logicOp = VK_LOGIC_OP_COPY;
	color_blending.attachmentCount = 1;
	color_blending.pAttachments = &colorBlendAttachment;

	// Dynamic state
	VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_LINE_WIDTH };

	dynamic_state.dynamicStateCount = BX_ARRAYSIZE(dynamicStates);
	dynamic_state.pDynamicStates = dynamicStates;

	// Attributes
	VkVertexInputAttributeDescription* attributes = darray_create(VkVertexInputAttributeDescription, MEMORY_TAG_RENDERER);
	VkVertexInputBindingDescription binding_desc = { 0 };

	if (shader->layout.attrib_count > 0 && shader->layout.initialized) {
		binding_desc.binding = 0;
		binding_desc.stride = box_render_layout_stride(&shader->layout);
		binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		for (u32 i = 0; i < box_render_layout_count(&shader->layout); ++i) {
			box_vertex_attrib_desc* attribute = &shader->layout.attribs[i];

			VkVertexInputAttributeDescription* descriptor = darray_push_empty(attributes);
			descriptor->binding = 0;
			descriptor->location = i;
			descriptor->format = box_format_to_vulkan_type(attribute->type);
			descriptor->offset = attribute->offset;
		}
	}
	
	vertex_input_info.vertexBindingDescriptionCount = 1;
	vertex_input_info.pVertexBindingDescriptions = &binding_desc;

	vertex_input_info.vertexAttributeDescriptionCount = darray_length(attributes);
	vertex_input_info.pVertexAttributeDescriptions = attributes;

	// Input assembly
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly.primitiveRestartEnable = VK_FALSE;

	// Pipeline layout
	VkResult result = create_pipeline_layout(context, shader, out_pipeline);
	if (!vulkan_result_is_success(result)) return result;

	// Pipeline create
	VkGraphicsPipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	pipelineInfo.stageCount = darray_length(shader_stages);
	pipelineInfo.pStages = shader_stages;
	pipelineInfo.pVertexInputState = &vertex_input_info;
	pipelineInfo.pInputAssemblyState = &input_assembly;

	pipelineInfo.pViewportState = &viewport_state;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depth_stencil;
	pipelineInfo.pColorBlendState = &color_blending;
	pipelineInfo.pDynamicState = &dynamic_state;
	pipelineInfo.pTessellationState = 0;

	pipelineInfo.layout = out_pipeline->layout;

	pipelineInfo.renderPass = renderpass->handle;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	result = vkCreateGraphicsPipelines(context->device.logical_device, VK_NULL_HANDLE, 1, &pipelineInfo, context->allocator, &out_pipeline->handle);
	if (!vulkan_result_is_success(result)) return result;

	for (u32 i = 0; i < darray_length(shader_stages); ++i) {
		vkDestroyShaderModule(context->device.logical_device, shader_stages[i].module, context->allocator);
	}

	out_pipeline->bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;

	darray_destroy(attributes);
	darray_destroy(shader_stages);
	return VK_SUCCESS;
}

VkResult vulkan_compute_pipeline_create(
	vulkan_context* context, 
	vulkan_pipeline* out_pipeline, 
	vulkan_renderpass* renderpass, 
	box_renderstage* shader) {
	// Fill shader stages with data from shader array
	shader_stage* stage = &shader->stages[BOX_SHADER_STAGE_TYPE_COMPUTE];
	if (stage->file_size == 0 || stage->file_data == NULL) {
		BX_ERROR("Renderstage does not has connected compute shader stage");
		return VK_ERROR_UNKNOWN;
	}

	VkShaderModule shader_module;
	VkShaderModuleCreateInfo create_info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	create_info.pCode = stage->file_data;
	create_info.codeSize = stage->file_size;
	VkResult result = vkCreateShaderModule(context->device.logical_device, &create_info, context->allocator, &shader_module);
	if (!vulkan_result_is_success(result)) return result;

	VkPipelineShaderStageCreateInfo stage_info = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	stage_info.stage = box_shader_type_to_vulkan_type(BOX_SHADER_STAGE_TYPE_COMPUTE);
	stage_info.module = shader_module;
	stage_info.pName = "main";

	bfree(stage->file_data, stage->file_size, MEMORY_TAG_CORE); // Comes from filesystem_read_entire_binary_file

	// Pipeline layout
	result = create_pipeline_layout(context, shader, out_pipeline);
	if (!vulkan_result_is_success(result)) return result;

	VkComputePipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.layout = out_pipeline->layout;
	pipelineInfo.stage = stage_info;

	result = vkCreateComputePipelines(context->device.logical_device, VK_NULL_HANDLE, 1, &pipelineInfo, context->allocator, &out_pipeline->handle);
	if (!vulkan_result_is_success(result)) return result;

	vkDestroyShaderModule(context->device.logical_device, shader_module, context->allocator);

	out_pipeline->bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
	return VK_SUCCESS;
}

void vulkan_pipeline_destroy(vulkan_context* context, vulkan_pipeline* pipeline) {
	if (pipeline->handle)
		vkDestroyPipeline(context->device.logical_device, pipeline->handle, context->allocator);

	if (pipeline->descriptor_sets) {
		darray_destroy(pipeline->descriptor_sets);
	}

	if (pipeline->descriptor)
		vkDestroyDescriptorSetLayout(context->device.logical_device, pipeline->descriptor, context->allocator);

	if (pipeline->descriptor_pool)
		vkDestroyDescriptorPool(context->device.logical_device, pipeline->descriptor_pool, context->allocator);

	if (pipeline->layout)
		vkDestroyPipelineLayout(context->device.logical_device, pipeline->layout, context->allocator);
}

void vulkan_pipeline_bind(vulkan_command_buffer* command_buffer, vulkan_context* context, vulkan_pipeline* pipeline) {
	vkCmdBindPipeline(command_buffer->handle, pipeline->bind_point, pipeline->handle);

	if (pipeline->descriptor_sets)
		vkCmdBindDescriptorSets(command_buffer->handle, pipeline->bind_point, pipeline->layout, 0, 1, &pipeline->descriptor_sets[context->image_index], 0, 0);
}