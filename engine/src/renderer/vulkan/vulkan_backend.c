#include "defines.h"
#include "vulkan_backend.h"

#include "engine.h"
#include "platform/filesystem.h"
#include "renderer/renderer_cmd.h"

#include "vulkan_types.h"
#include "vulkan_device.h"
#include "vulkan_buffer.h"
#include "vulkan_swapchain.h"
#include "vulkan_renderpass.h"
#include "vulkan_image.h"
#include "vulkan_pipeline.h"
#include "vulkan_framebuffer.h"
#include "vulkan_command_buffer.h"
#include "vulkan_fence.h"

VkResult platform_create_vulkan_surface(vulkan_context* context, box_platform* plat_state);

VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
	VkDebugUtilsMessageTypeFlagsEXT message_types,
	const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
	void* user_data) {
	switch (message_severity) {
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		BX_ERROR(callback_data->pMessage);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		BX_WARN(callback_data->pMessage);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		BX_INFO(callback_data->pMessage);
		break;

	default:
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
		BX_TRACE(callback_data->pMessage);
		break;
	}

	return VK_FALSE;
}

b8 vulkan_renderer_backend_initialize(box_renderer_backend* backend, uvec2 starting_size, const char* application_name) {
	backend->internal_context = ballocate(sizeof(vulkan_context), MEMORY_TAG_RENDERER);

	vulkan_context* context = (vulkan_context*)backend->internal_context;
	context->framebuffer_size = (vec2){ starting_size.x, starting_size.y };
	context->allocator = 0; // TODO: Custom allocator
	
	VkApplicationInfo app_info = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app_info.apiVersion = VK_API_VERSION_1_2; // TODO: Does it need to be this low??
	app_info.pApplicationName = application_name;
	app_info.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
	app_info.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
	app_info.pEngineName = "Boxel";

	// Obtain a list of required extensions
	const char** required_extensions = darray_create(const char*, MEMORY_TAG_RENDERER);
	const char** required_validation_layer_names = darray_create(const char*, MEMORY_TAG_RENDERER);

	// Platform-specific extension(s)
	platform_get_vulkan_extensions(&required_extensions);

	// Add debug extensions/layers if enabled
	if (backend->config.enable_validation) {
		darray_push(required_extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		darray_push(required_validation_layer_names, "VK_LAYER_KHRONOS_validation");
	}

	// Fill create info
	VkInstanceCreateInfo create_info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	create_info.pApplicationInfo = &app_info;
	create_info.enabledExtensionCount = darray_length(required_extensions); // TODO: Verify existence of extensions.
	create_info.ppEnabledExtensionNames = required_extensions;              // TODO: Verify existence of validation layers.
	create_info.enabledLayerCount = darray_length(required_validation_layer_names);
	create_info.ppEnabledLayerNames = required_validation_layer_names;

	BX_TRACE("Required extensions:");
	for (u32 i = 0; i < darray_length(required_extensions); ++i) {
		BX_TRACE("    %s", required_extensions[i]);
	}

	CHECK_VKRESULT(
		vkCreateInstance(&create_info, context->allocator, &context->instance), 
		"Failed to create Vulkan instance");

	BX_INFO("Vulkan Instance created.");

	// Clean up temp arrays
	darray_destroy(required_extensions);
	darray_destroy(required_validation_layer_names);

	// Setup debug messenger if validation enabled
	if (backend->config.enable_validation) {
		u32 log_severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;

		VkDebugUtilsMessengerCreateInfoEXT debug_create_info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
		debug_create_info.messageSeverity = log_severity;
		debug_create_info.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
		debug_create_info.pfnUserCallback = vk_debug_callback;

		PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context->instance, "vkCreateDebugUtilsMessengerEXT");

		CHECK_VKRESULT(
			func(context->instance, &debug_create_info, context->allocator, &context->debug_messenger),
			"Failed to create Vulkan debug messanger");
	}

	// Create the Vulkan surface
	CHECK_VKRESULT(
		platform_create_vulkan_surface(context, backend->plat_state),
		"Failed to create Vulkan platform surface");

	BX_INFO("Vulkan surface created.");

	// Create the Vulkan device
	CHECK_VKRESULT(
		vulkan_device_create(backend),
		"Failed to create Vulkan device");

	BX_INFO("Vulkan device created.");

	// Create the Vulkan swapchain
	CHECK_VKRESULT(
		vulkan_swapchain_create(backend, context->framebuffer_size, &context->swapchain),
		"Failed to create Vulkan swapchain");

	CHECK_VKRESULT(
		vulkan_renderpass_create(context, &context->main_renderpass, (vec2) { 0, 0 }, context->framebuffer_size),
		"Failed to create main Vulkan renderpass");

	// Create intermediate objects.
	context->swapchain.framebuffers = darray_reserve(vulkan_framebuffer, context->swapchain.image_count, MEMORY_TAG_RENDERER);
	context->images_in_flight = darray_reserve(vulkan_fence*, context->swapchain.image_count, MEMORY_TAG_RENDERER);

	context->image_available_semaphores = darray_reserve(VkSemaphore, backend->config.frames_in_flight, MEMORY_TAG_RENDERER);
	context->queue_complete_semaphores = darray_reserve(VkSemaphore, backend->config.frames_in_flight, MEMORY_TAG_RENDERER);
	context->in_flight_fences = darray_reserve(vulkan_fence, backend->config.frames_in_flight, MEMORY_TAG_RENDERER);

	// Create framebuffers & command buffers.
	for (u32 i = 0; i < context->swapchain.image_count; ++i) {
		// TODO: make this dynamic based on the currently configured attachments
		VkImageView attachments[] = { context->swapchain.views[i] };

		CHECK_VKRESULT(
			vulkan_framebuffer_create(
				context,
				&context->main_renderpass,
				context->framebuffer_size,
				BX_ARRAYSIZE(attachments), attachments,
				&context->swapchain.framebuffers[i]),
			"Failed to create Vulkan framebuffers");
	}

	if (backend->config.modes & RENDERER_MODE_GRAPHICS) {
		vulkan_queue_type queue_type = VULKAN_QUEUE_TYPE_GRAPHICS;
		context->command_buffer_ring[queue_type] = darray_reserve(vulkan_command_buffer, context->swapchain.image_count, MEMORY_TAG_RENDERER);

		for (u32 i = 0; i < context->swapchain.image_count; ++i) {
			// TODO: Allocate command buffer all at once.
			CHECK_VKRESULT(
				vulkan_command_buffer_allocate(
					context,
					context->device.mode_queues[queue_type].pool,
					TRUE,
					&context->command_buffer_ring[queue_type][i]),
				"Failed to create Vulkan command buffers");

		}
	}

	if (backend->config.modes & RENDERER_MODE_COMPUTE) {
		vulkan_queue_type queue_type = VULKAN_QUEUE_TYPE_COMPUTE;
		context->command_buffer_ring[queue_type] = darray_reserve(vulkan_command_buffer, context->swapchain.image_count, MEMORY_TAG_RENDERER);

		for (u32 i = 0; i < context->swapchain.image_count; ++i) {
			// TODO: Allocate command buffer all at once.
			CHECK_VKRESULT(
				vulkan_command_buffer_allocate(
					context,
					context->device.mode_queues[queue_type].pool,
					TRUE,
					&context->command_buffer_ring[queue_type][i]),
				"Failed to create Vulkan command buffers");
		}
	}

	BX_TRACE("Vulkan command buffers created.");

	for (u8 i = 0; i < backend->config.frames_in_flight; ++i) {
		VkSemaphoreCreateInfo semaphore_create_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		CHECK_VKRESULT(
			vkCreateSemaphore(
				context->device.logical_device, 
				&semaphore_create_info, 
				context->allocator,
				&context->image_available_semaphores[i]),
			"Failed to create Vulkan sync objects");

		CHECK_VKRESULT(
			vkCreateSemaphore(
				context->device.logical_device, 
				&semaphore_create_info, 
				context->allocator, 
				&context->queue_complete_semaphores[i]),
			"Failed to create Vulkan sync objects");

		// Create the fence in a signaled state, indicating that the first frame has already been "rendered".
		// This will prevent the application from waiting indefinitely for the first frame to render since it
		// cannot be rendered until a frame is "rendered" before it.
		CHECK_VKRESULT(
			vulkan_fence_create(
				context, 
				TRUE, 
				&context->in_flight_fences[i]),
			"Failed to create Vulkan sync objects");
	}

	BX_INFO("Vulkan renderer initialized successfully.");
	return TRUE;
}

void vulkan_renderer_backend_shutdown(box_renderer_backend* backend) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;
	vulkan_renderer_backend_wait_until_idle(backend, UINT64_MAX);

	// Destroy in the opposite order of creation.

	// Create framebuffers & command buffers.
	for (u32 i = 0; i < context->swapchain.image_count; ++i) {
		vulkan_framebuffer_destroy(
			context, 
			&context->swapchain.framebuffers[i]);
	}

	for (u32 i = 0; i < BX_ARRAYSIZE(context->command_buffer_ring); ++i) {
		if (!context->command_buffer_ring[i]) continue;

		for (u32 j = 0; j < darray_length(context->command_buffer_ring[i]); ++j) {
			vulkan_command_buffer_free(
				context,
				&context->command_buffer_ring[i][j]);

			vulkan_command_buffer_free(
				context,
				&context->command_buffer_ring[i][j]);
		}

		darray_destroy(context->command_buffer_ring[i]);
	}

	for (u8 i = 0; i < backend->config.frames_in_flight; ++i) {
		if (context->image_available_semaphores[i]) {
			vkDestroySemaphore(
				context->device.logical_device,
				context->image_available_semaphores[i],
				context->allocator);
		}
		
		if (context->queue_complete_semaphores[i]) {
			vkDestroySemaphore(
				context->device.logical_device,
				context->queue_complete_semaphores[i],
				context->allocator);
		}

		vulkan_fence_destroy(
			context,
			&context->in_flight_fences[i]);
	}

	darray_destroy(context->swapchain.framebuffers);
	darray_destroy(context->images_in_flight);
	darray_destroy(context->image_available_semaphores);
	darray_destroy(context->queue_complete_semaphores);
	darray_destroy(context->in_flight_fences);

	vulkan_renderpass_destroy(context, &context->main_renderpass);

	vulkan_swapchain_destroy(backend, &context->swapchain);

	vulkan_device_destroy(backend);

	if (context->surface) vkDestroySurfaceKHR(context->instance, context->surface, context->allocator);

	if (context->instance) {
		PFN_vkDestroyDebugUtilsMessengerEXT func =
			(PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context->instance, "vkDestroyDebugUtilsMessengerEXT");
		func(context->instance, context->debug_messenger, context->allocator);

		vkDestroyInstance(context->instance, context->allocator);
	}

	bfree(context, sizeof(vulkan_context), MEMORY_TAG_RENDERER);
	backend->internal_context = NULL;
}

void vulkan_renderer_backend_wait_until_idle(box_renderer_backend* backend, u64 timeout) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;
	if (context->device.logical_device) vkDeviceWaitIdle(context->device.logical_device);
}

void vulkan_renderer_backend_on_resized(box_renderer_backend* backend, u32 width, u32 height) {
	// TODO: Swapchain recreation.
}

b8 vulkan_renderer_backend_begin_frame(box_renderer_backend* backend, f32 delta_time) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;

	vulkan_fence_wait(context, &context->in_flight_fences[context->current_frame], UINT64_MAX);
	vulkan_fence_reset(context, &context->in_flight_fences[context->current_frame]);

	if (backend->config.modes & RENDERER_MODE_GRAPHICS) {
		// Acquire next swapchain image
		CHECK_VKRESULT(
			vulkan_swapchain_acquire_next_image_index(
				context,
				&context->swapchain,
				UINT64_MAX,
				context->image_available_semaphores[context->current_frame],
				0,
				&context->image_index),
			"Failed to acquire next Vulkan swapchain image");
	}

	// If this swapchain image is still in flight, wait for it
	if (context->images_in_flight[context->image_index] != VK_NULL_HANDLE)
		vulkan_fence_wait(context, context->images_in_flight[context->image_index], UINT64_MAX);

	context->images_in_flight[context->image_index] = &context->in_flight_fences[context->current_frame];

	for (u32 i = 0; i < BX_ARRAYSIZE(context->command_buffer_ring); ++i) {
		vulkan_command_buffer* command_ring = context->command_buffer_ring[i];
		if (!command_ring) continue;

		vulkan_command_buffer* cmd = &command_ring[context->current_frame];
		vulkan_command_buffer_reset(cmd);
		vulkan_command_buffer_begin(cmd, FALSE, FALSE, FALSE);
	}

	return TRUE;
}

vulkan_queue_type box_renderer_mode_to_queue_type(renderer_mode mode) {
	switch (mode) {
	case RENDERER_MODE_GRAPHICS: return VULKAN_QUEUE_TYPE_GRAPHICS;
	case RENDERER_MODE_COMPUTE: return VULKAN_QUEUE_TYPE_COMPUTE;
	case RENDERER_MODE_TRANSFER: return VULKAN_QUEUE_TYPE_TRANSFER;
	}

	return 0;
}

void vulkan_renderer_playback_rendercmd(box_renderer_backend* backend, box_rendercmd_context* rendercmd_context, rendercmd_header* header, rendercmd_payload* payload) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;
	box_renderstage* current_shader = rendercmd_context->current_shader;

	if (header->supported_mode != 0) {
		rendercmd_context->command_buffer = &context->command_buffer_ring[box_renderer_mode_to_queue_type(header->supported_mode)][context->current_frame];
		rendercmd_context->command_buffer->used = TRUE;
	}

	BX_ASSERT(rendercmd_context->command_buffer != NULL);

	vulkan_pipeline* pipeline = NULL;
	if (current_shader)
		pipeline = (vulkan_pipeline*)current_shader->internal_data;

	switch (header->type) {
	case RENDERCMD_SET_CLEAR_COLOUR:
		// Set dynamic state, begin render pass, etc.
		VkViewport viewport;
		viewport.x = 0.0f;
		viewport.y = (f32)context->framebuffer_size.height;
		viewport.width = (f32)context->framebuffer_size.width;
		viewport.height = -(f32)context->framebuffer_size.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		// Scissor
		VkRect2D scissor;
		scissor.offset.x = scissor.offset.y = 0;
		scissor.extent.width = context->framebuffer_size.width;
		scissor.extent.height = context->framebuffer_size.height;

		vkCmdSetViewport(rendercmd_context->command_buffer->handle, 0, 1, &viewport);
		vkCmdSetScissor(rendercmd_context->command_buffer->handle, 0, 1, &scissor);

		context->main_renderpass.clear_colour = payload->set_clear_colour.clear_colour;
		vulkan_renderpass_begin(rendercmd_context->command_buffer, &context->main_renderpass, &context->swapchain.framebuffers[context->image_index]);
		break;

	case RENDERCMD_BEGIN_RENDERSTAGE:
		vulkan_pipeline_bind(rendercmd_context->command_buffer, context, pipeline);
		break;

	case RENDERCMD_BIND_BUFFER:
		box_renderbuffer* buffer = payload->bind_buffer.renderbuffer;
		vulkan_buffer* vk_buffer = (vulkan_buffer*)buffer->internal_data;

		switch (payload->bind_buffer.renderbuffer->usage) {
		case BOX_RENDERBUFFER_USAGE_VERTEX:
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(
				rendercmd_context->command_buffer->handle,
				payload->bind_buffer.binding,
				1,
				&vk_buffer->handle,
				&offset);
			break;

		case BOX_RENDERBUFFER_USAGE_INDEX:
			vkCmdBindIndexBuffer(
				rendercmd_context->command_buffer->handle,
				vk_buffer->handle,
				0,
				box_data_type_to_vulkan_index_type(rendercmd_context->current_shader->layout.index_type));
			break;
		
		// TODO: REMOVE!
		case BOX_RENDERBUFFER_USAGE_STORAGE:
			VkDescriptorBufferInfo bufferInfo = { 0 };
			bufferInfo.buffer = vk_buffer->handle;
			bufferInfo.offset = 0;
			bufferInfo.range = VK_WHOLE_SIZE;

			vulkan_pipeline* bound_pipeline = (vulkan_pipeline*)rendercmd_context->current_shader->internal_data;

			// Update descriptor sets.
			VkWriteDescriptorSet descriptor_write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptor_write.dstSet = bound_pipeline->descriptor_sets[context->image_index];
			descriptor_write.dstBinding = payload->bind_buffer.binding;
			descriptor_write.dstArrayElement = 0;
			descriptor_write.descriptorType = box_renderbuffer_usage_to_vulkan_type(payload->bind_buffer.renderbuffer->usage);
			descriptor_write.descriptorCount = 1;
			descriptor_write.pBufferInfo = &bufferInfo;

			vkUpdateDescriptorSets(context->device.logical_device, 1, &descriptor_write, 0, 0);
			break;
		}
		break;

	case RENDERCMD_DRAW:
		vkCmdDraw(
			rendercmd_context->command_buffer->handle,
			payload->draw.vertex_count,
			payload->draw.instance_count,
			0, 0);
		break;

	case RENDERCMD_DRAW_INDEXED:
		vkCmdDrawIndexed(
			rendercmd_context->command_buffer->handle,
			payload->draw_indexed.index_count,
			payload->draw_indexed.instance_count,
			0, 0, 0);
		break;

	case RENDERCMD_DISPATCH:
		vkCmdDispatch(
			rendercmd_context->command_buffer->handle,
			payload->dispatch.group_size.x,
			payload->dispatch.group_size.y,
			payload->dispatch.group_size.z);
		break;
		
	case _RENDERCMD_END:
		if (context->main_renderpass.state == RENDER_PASS_STATE_RECORDING)
			vulkan_renderpass_end(rendercmd_context->command_buffer, &context->main_renderpass);
		break;
	}
}

b8 vulkan_renderer_backend_end_frame(box_renderer_backend* backend) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;

	for (u32 i = 0; i < BX_ARRAYSIZE(context->command_buffer_ring); ++i) {
		vulkan_command_buffer* command_ring = context->command_buffer_ring[i];
		if (!command_ring) continue;

		vulkan_command_buffer* cmd = &command_ring[context->current_frame];
		b8 used_command_buffer = cmd->used;

		vulkan_command_buffer_end(cmd);
		cmd->used = FALSE;

		if (!used_command_buffer) continue;

		// Submit into queues
		VkSemaphore wait_semaphores[] = { context->image_available_semaphores[context->current_frame] };
		VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };
		VkSemaphore signal_semaphores[] = { context->queue_complete_semaphores[context->current_frame] };

		VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submit_info.pWaitDstStageMask = wait_stages;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &cmd->handle;
		submit_info.signalSemaphoreCount = BX_ARRAYSIZE(signal_semaphores);
		submit_info.pSignalSemaphores = signal_semaphores;
		submit_info.waitSemaphoreCount = BX_ARRAYSIZE(wait_semaphores);
		submit_info.pWaitSemaphores = wait_semaphores;

		CHECK_VKRESULT(
			vkQueueSubmit(
				context->device.mode_queues[i].handle,
				1,
				&submit_info,
				context->in_flight_fences[context->current_frame].handle),
			"Failed to submit Vulkan command buffers to driver");

		vulkan_command_buffer_update_submitted(cmd);
	}

	if (backend->config.modes & RENDERER_MODE_GRAPHICS) {
		// Present
		CHECK_VKRESULT(
			vulkan_swapchain_present(
				context,
				&context->swapchain,
				context->device.mode_queues[VULKAN_QUEUE_TYPE_PRESENT].handle,
				context->queue_complete_semaphores[context->current_frame],
				context->image_index),
			"Failed to present Vulkan swapchain");
	}

	// Advance frame index
	context->current_frame = (context->current_frame + 1) % backend->config.frames_in_flight;
	return TRUE;
}

b8 vulkan_renderer_create_renderstage(box_renderer_backend* backend, box_renderstage* out_stage) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;

	out_stage->internal_data = ballocate(sizeof(vulkan_pipeline), MEMORY_TAG_RENDERER);

	switch (out_stage->mode) {
	case RENDERER_MODE_GRAPHICS:
		CHECK_VKRESULT(
			vulkan_graphics_pipeline_create(
				context,
				out_stage->internal_data,
				&context->main_renderpass,
				out_stage),
			"Failed to create Vulkan graphics pipeline");
		break;

	case RENDERER_MODE_COMPUTE:
		CHECK_VKRESULT(
			vulkan_compute_pipeline_create(
				context,
				out_stage->internal_data,
				&context->main_renderpass,
				out_stage),
			"Failed to create Vulkan compute pipeline");
		break;
	}

	return TRUE;
}

void vulkan_renderer_destroy_renderstage(box_renderer_backend* backend, box_renderstage* out_stage) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;

	if (out_stage->internal_data != NULL) {
		vulkan_pipeline* pipeline = (vulkan_pipeline*)out_stage->internal_data;
		vulkan_pipeline_destroy(context, pipeline);
 		bfree(pipeline, sizeof(vulkan_pipeline), MEMORY_TAG_RENDERER);
	}
}

b8 vulkan_renderer_create_renderbuffer(box_renderer_backend* backend, box_renderbuffer* out_buffer) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;

	out_buffer->internal_data = ballocate(sizeof(vulkan_buffer), MEMORY_TAG_RENDERER);
	vulkan_buffer* buffer = (vulkan_buffer*)out_buffer->internal_data;

	VkBufferUsageFlagBits buffer_usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	if (out_buffer->usage & BOX_RENDERBUFFER_USAGE_VERTEX)
		buffer_usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	if (out_buffer->usage & BOX_RENDERBUFFER_USAGE_INDEX)
		buffer_usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	if (out_buffer->usage & BOX_RENDERBUFFER_USAGE_STORAGE)
		buffer_usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	CHECK_VKRESULT(
		vulkan_buffer_create(
			context,
			out_buffer->buffer_size,
			buffer_usage,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			buffer),
		"Failed to create internal Vulkan renderbuffer");

	return TRUE;
}

b8 vulkan_renderer_upload_to_renderbuffer(box_renderer_backend* backend, box_renderbuffer* buffer, void* data, u64 start_offset, u64 region) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;

#if BOX_ENABLE_VALIDATION
	if (!(backend->config.modes & RENDERER_MODE_TRANSFER)) {
		BX_ERROR("Attempting to upload to box_renderbuffer without enabling transfer mode.");
		return FALSE;
	}
#endif

	vulkan_buffer* internal_buffer = (vulkan_buffer*)buffer->internal_data;

	vulkan_buffer staging_buffer = { 0 };
	CHECK_VKRESULT(
		vulkan_buffer_create(
			context,
			buffer->buffer_size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&staging_buffer),
		"Failed to create Vulkan staging buffer");

	vulkan_buffer_map_data(context, &staging_buffer, buffer->buffer_size, data);

	vulkan_queue* selected_mode = &context->device.mode_queues[VULKAN_QUEUE_TYPE_TRANSFER];

	vulkan_command_buffer command_buffer;
	vulkan_command_buffer_allocate_and_begin_single_use(context, selected_mode->pool, &command_buffer);

	VkBufferCopy copy_info = { 0 };
	copy_info.size = region;
	copy_info.dstOffset = start_offset;
	vkCmdCopyBuffer(command_buffer.handle, staging_buffer.handle, internal_buffer->handle, 1, &copy_info);

	CHECK_VKRESULT(
		vulkan_command_buffer_end_single_use(
			context,
			&command_buffer,
			selected_mode->handle),
		"Failed to transfer Vulkan renderbuffer to GPU");

	vulkan_buffer_destroy(context, &staging_buffer);
	return TRUE;
}

void vulkan_renderer_destroy_renderbuffer(box_renderer_backend* backend, box_renderbuffer* out_buffer) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;

	if (out_buffer->internal_data != NULL) {
		vulkan_buffer* buffer = (vulkan_buffer*)out_buffer->internal_data;
		vulkan_buffer_destroy(context, buffer);
		bfree(buffer, sizeof(vulkan_buffer), MEMORY_TAG_RENDERER);
	}
}

b8 vulkan_renderer_create_texture(box_renderer_backend* backend, box_texture* out_texture) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;

	out_texture->internal_data = ballocate(sizeof(vulkan_image), MEMORY_TAG_RENDERER);
	vulkan_image* image = (vulkan_image*)out_texture->internal_data;

	CHECK_VKRESULT(
		vulkan_image_create(
			context, 
			VK_IMAGE_TYPE_2D, 
			out_texture->size,
			box_attribute_to_vulkan_type(out_texture->image_format),
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
			TRUE,
			TRUE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_ASPECT_COLOR_BIT,
			(backend->config.sampler_anisotropy ? backend->config.capabilities.max_anisotropy : 0.0f),
			image),
		"Failed to create internal Vulkan image");

	if (out_texture->temp_user_data != NULL) {
#if BOX_ENABLE_VALIDATION
		if (!(backend->config.modes & RENDERER_MODE_TRANSFER)) {
			BX_ERROR("Attempting to upload to box_renderbuffer without enabling transfer mode.");
			return FALSE;
		}
#endif
		 
		u64 image_size = out_texture->size.x * out_texture->size.y * out_texture->image_format.channel_count;

		vulkan_buffer staging_buffer = { 0 };
		CHECK_VKRESULT(
			vulkan_buffer_create(
				context,
				image_size,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&staging_buffer),
			"Failed to create Vulkan staging buffer");

		vulkan_buffer_map_data(context, &staging_buffer, image_size, out_texture->temp_user_data);

		vulkan_queue* selected_mode = &context->device.mode_queues[VULKAN_QUEUE_TYPE_TRANSFER];
		vulkan_command_buffer command_buffer;
		vulkan_command_buffer_allocate_and_begin_single_use(context, selected_mode->pool, &command_buffer);

		vulkan_image_transition_format(&command_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		VkBufferImageCopy copy_info = { 0 };
		copy_info.imageExtent = (VkExtent3D){ out_texture->size.x, out_texture->size.y, 1 };
		copy_info.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy_info.imageSubresource.layerCount = 1;
		vkCmdCopyBufferToImage(command_buffer.handle, staging_buffer.handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_info);

		vulkan_image_transition_format(&command_buffer, image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		CHECK_VKRESULT(
			vulkan_command_buffer_end_single_use(
				context,
				&command_buffer,
				selected_mode->handle),
			"Failed to transfer Vulkan renderbuffer to GPU");

		vulkan_buffer_destroy(context, &staging_buffer);
	}
	return TRUE;
}

void vulkan_renderer_destroy_texture(box_renderer_backend* backend, box_texture* texture) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;

	if (texture->internal_data != NULL) {
		vulkan_image* image = (vulkan_image*)texture->internal_data;
		vulkan_image_destroy(context, image);
		bfree(image, sizeof(vulkan_image), MEMORY_TAG_RENDERER);
	}
}
