#include "defines.h"
#include "vulkan_pipeline.h"

#include "renderer/vertex_layout.h"

VkShaderStageFlagBits box_shader_type_to_vulkan_type(box_shader_stage_type type) {
	switch (type) {
	case BOX_SHADER_STAGE_TYPE_VERTEX:  return VK_SHADER_STAGE_VERTEX_BIT;
	case BOX_SHADER_STAGE_TYPE_GEOMETRY: return VK_SHADER_STAGE_GEOMETRY_BIT;
	case BOX_SHADER_STAGE_TYPE_FRAGMENT: return VK_SHADER_STAGE_FRAGMENT_BIT;
	case BOX_SHADER_STAGE_TYPE_COMPUTE:  return VK_SHADER_STAGE_COMPUTE_BIT;
	}

	return 0;
}

VkFormat box_attribute_to_vulkan_type(box_vertex_attrib_type type, u64 count) {
	// Vulkan supports 1-4 components only
	if (count < 1 || count > 4) {
		return VK_FORMAT_UNDEFINED;
	}

	switch (type) {
	case BOX_VERTEX_ATTRIB_SINT8:
		if (count == 1) return VK_FORMAT_R8_SINT;
		if (count == 2) return VK_FORMAT_R8G8_SINT;
		if (count == 3) return VK_FORMAT_R8G8B8_SINT;
		if (count == 4) return VK_FORMAT_R8G8B8A8_SINT;

	case BOX_VERTEX_ATTRIB_SINT16:
		if (count == 1) return VK_FORMAT_R16_SINT;
		if (count == 2) return VK_FORMAT_R16G16_SINT;
		if (count == 3) return VK_FORMAT_R16G16B16_SINT;
		if (count == 4) return VK_FORMAT_R16G16B16A16_SINT;

	case BOX_VERTEX_ATTRIB_SINT32:
		if (count == 1) return VK_FORMAT_R32_SINT;
		if (count == 2) return VK_FORMAT_R32G32_SINT;
		if (count == 3) return VK_FORMAT_R32G32B32_SINT;
		if (count == 4) return VK_FORMAT_R32G32B32A32_SINT;

	case BOX_VERTEX_ATTRIB_UINT8:
		if (count == 1) return VK_FORMAT_R8_UINT;
		if (count == 2) return VK_FORMAT_R8G8_UINT;
		if (count == 3) return VK_FORMAT_R8G8B8_UINT;
		if (count == 4) return VK_FORMAT_R8G8B8A8_UINT;

	case BOX_VERTEX_ATTRIB_UINT16:
		if (count == 1) return VK_FORMAT_R16_UINT;
		if (count == 2) return VK_FORMAT_R16G16_UINT;
		if (count == 3) return VK_FORMAT_R16G16B16_UINT;
		if (count == 4) return VK_FORMAT_R16G16B16A16_UINT;

	case BOX_VERTEX_ATTRIB_BOOL:
	case BOX_VERTEX_ATTRIB_UINT32:
		if (count == 1) return VK_FORMAT_R32_UINT;
		if (count == 2) return VK_FORMAT_R32G32_UINT;
		if (count == 3) return VK_FORMAT_R32G32B32_UINT;
		if (count == 4) return VK_FORMAT_R32G32B32A32_UINT;

	case BOX_VERTEX_ATTRIB_FLOAT32:
		if (count == 1) return VK_FORMAT_R32_SFLOAT;
		if (count == 2) return VK_FORMAT_R32G32_SFLOAT;
		if (count == 3) return VK_FORMAT_R32G32B32_SFLOAT;
		if (count == 4) return VK_FORMAT_R32G32B32A32_SFLOAT;
	}

	return VK_FORMAT_UNDEFINED;
}

VkResult vulkan_graphics_pipeline_create(
	vulkan_context* context, 
	vulkan_graphics_pipeline* out_pipeline, 
	vulkan_renderpass* renderpass, 
	box_renderstage* shader) {
	// Fill shader stages with data from shader array
	VkPipelineShaderStageCreateInfo* shader_stages = darray_create(VkPipelineShaderStageCreateInfo, MEMORY_TAG_RENDERER);
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
		VkResult result = vkCreateShaderModule(context->device.logical_device, &create_info, context->allocator, &shader_module);
		if (!vulkan_result_is_success(result)) return result;

		VkPipelineShaderStageCreateInfo stage_info = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
		stage_info.stage = box_shader_type_to_vulkan_type((box_shader_stage_type)i);
		stage_info.module = shader_module;
		stage_info.pName = "main";
		shader_stages = _darray_push(shader_stages, &stage_info);

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
	depth_stencil.depthTestEnable = shader->depth_test;
	depth_stencil.depthWriteEnable = shader->depth_test;
	depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depth_stencil.depthBoundsTestEnable = VK_FALSE;
	depth_stencil.stencilTestEnable = VK_FALSE;

	// Colour attachments 
	VkPipelineColorBlendAttachmentState colorBlendAttachment = { 0 };
	colorBlendAttachment.blendEnable = shader->blending;
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

	if (shader->layout.initialized) {
		binding_desc.binding = 0;
		binding_desc.stride = box_vertex_layout_stride(&shader->layout);
		binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		for (u32 i = 0; i < box_vertex_layout_count(&shader->layout); ++i) {
			box_vertex_attrib_desc* attribute = &shader->layout.attribs[i];

			VkVertexInputAttributeDescription descriptor = { 0 };
			descriptor.binding = 0;
			descriptor.location = i;
			descriptor.format = box_attribute_to_vulkan_type(attribute->type, attribute->count);
			descriptor.offset = attribute->offset;
			attributes = _darray_push(attributes, &descriptor);
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
	VkPipelineLayoutCreateInfo layout_info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	layout_info.setLayoutCount = 0;
	layout_info.pushConstantRangeCount = 0;

	VkResult result = vkCreatePipelineLayout(context->device.logical_device, &layout_info, context->allocator, &out_pipeline->layout);
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

	darray_destroy(attributes);
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