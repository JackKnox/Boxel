#include "defines.h"
#include "vulkan_pipeline.h"

VkShaderStageFlagBits box_shader_type_to_vulkan_type(box_shader_stage_type type) {
	switch (type) {
	case BOX_SHADER_STAGE_TYPE_VERTEX: return VK_SHADER_STAGE_VERTEX_BIT;
	case BOX_SHADER_STAGE_TYPE_GEOMETRY: return VK_SHADER_STAGE_GEOMETRY_BIT;
	case BOX_SHADER_STAGE_TYPE_FRAGMENT: return VK_SHADER_STAGE_FRAGMENT_BIT;
	case BOX_SHADER_STAGE_TYPE_COMPUTE: return VK_SHADER_STAGE_COMPUTE_BIT;
	}

	return 0;
}

b8 vulkan_graphics_pipeline_create(
	vulkan_context* context, 
	vulkan_graphics_pipeline* out_pipeline, 
	vulkan_renderpass* renderpass, 
	box_shader* shader) {
	// Fill shader stages with data from shader array
	VkPipelineShaderStageCreateInfo* shader_stages = darray_create(VkPipelineShaderStageCreateInfo);
	for (int i = 0; i < BOX_SHADER_STAGE_TYPE_MAX; ++i) {
		shader_stage* stage = &shader->stages[i];
		if (stage->file_size == 0) {
			if (stage->file_data != NULL) {
				BX_WARN("Shader stage says file size is zero but contains data?");
			}
			continue;
		}

		VkShaderModule shader_module;
		VkShaderModuleCreateInfo create_info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		create_info.pCode = stage->file_data;
		create_info.codeSize = stage->file_size;
		if (!vulkan_result_is_success(vkCreateShaderModule(
			context->device.logical_device, &create_info, 
			context->allocator, &shader_module))) {
			// ...
			return FALSE;
		}

		VkPipelineShaderStageCreateInfo stage_info = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
		stage_info.stage = box_shader_type_to_vulkan_type((box_shader_stage_type)i);
		stage_info.module = shader_module;
		stage_info.pName = "main";
		shader_stages = _darray_push(shader_stages, &stage_info);
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
	viewport.y = (f32)renderpass->h;
	viewport.width = (f32)renderpass->w;
	viewport.height = -(f32)renderpass->h;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	// Scissor
	VkRect2D scissor;
	scissor.offset.x = scissor.offset.y = 0;
	scissor.extent.width = renderpass->w;
	scissor.extent.height = renderpass->h;

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
	depth_stencil.depthTestEnable = VK_TRUE;
	depth_stencil.depthWriteEnable = VK_TRUE;
	depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depth_stencil.depthBoundsTestEnable = VK_FALSE;
	depth_stencil.stencilTestEnable = VK_FALSE;

	// Colour attachments 
	VkPipelineColorBlendAttachmentState colorBlendAttachment = { 0 };
	colorBlendAttachment.blendEnable = VK_TRUE;
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
	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
		VK_DYNAMIC_STATE_LINE_WIDTH };

	dynamic_state.dynamicStateCount = 3;
	dynamic_state.pDynamicStates = dynamicStates;

	// Attributes
	vertex_input_info.vertexBindingDescriptionCount = 0;
	vertex_input_info.vertexAttributeDescriptionCount = 0;

	// Input assembly
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly.primitiveRestartEnable = VK_FALSE;

	// Pipeline layout
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipelineLayoutInfo.setLayoutCount = 0;
	pipelineLayoutInfo.pushConstantRangeCount = 0;

	if (!vulkan_result_is_success(
		vkCreatePipelineLayout(context->device.logical_device,
		&pipelineLayoutInfo, context->allocator, &out_pipeline->layout))) {
		BX_ERROR("Failed to create pipeline layout!");
		return FALSE;
	}

	// Pipeline create
	VkGraphicsPipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	pipelineInfo.stageCount = darray_capacity(shader_stages);
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

	if (!vulkan_result_is_success(
		vkCreateGraphicsPipelines(context->device.logical_device,
		VK_NULL_HANDLE, 1, &pipelineInfo, context->allocator,
		&out_pipeline->handle))) {
		BX_ERROR("Failed to create graphics pipeline!");
		return FALSE;
	}

	darray_destroy(shader_stages);
	return TRUE;
}

void vulkan_graphics_pipeline_destroy(vulkan_context* context, vulkan_graphics_pipeline* pipeline) {
	if (pipeline->handle) 
		vkDestroyPipeline(context->device.logical_device, pipeline->handle, context->allocator);

	if (pipeline->layout)
		vkDestroyPipelineLayout(context->device.logical_device, pipeline->layout, context->allocator);
}

void vulkan_graphics_pipeline_bind(vulkan_command_buffer* command_buffer, vulkan_graphics_pipeline* pipeline) {
	vkCmdBindPipeline(command_buffer->handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->handle);
}