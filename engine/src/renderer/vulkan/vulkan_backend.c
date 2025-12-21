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
#include "vulkan_pipeline.h"
#include "vulkan_framebuffer.h"
#include "vulkan_command_buffer.h"
#include "vulkan_fence.h"

#define VERBOSE_ERRORS TRUE

#define CHECK_VKRESULT(func, message) { VkResult result = func; if (!vulkan_result_is_success(result)) { BX_ERROR(message ": %s", vulkan_result_string(result, VERBOSE_ERRORS)); return FALSE; } }

VkResult platform_create_vulkan_surface(vulkan_context* context, box_platform* plat_state);

VkResult vulkan_regenerate_framebuffer(vulkan_context* context);
VkResult vulkan_create_command_buffers(vulkan_context* context);

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
		"Failed to create platform surface");

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
		vulkan_renderpass_create(context, &context->main_renderpass, (vec2) { 0, 0 }, context->framebuffer_size, 0),
		"Failed to create main Vulkan renderpass");

	context->swapchain.framebuffers = darray_reserve(vulkan_framebuffer, context->swapchain.image_count, MEMORY_TAG_RENDERER);

	CHECK_VKRESULT(
		vulkan_regenerate_framebuffer(context),
		"Failed to create Vulkan framebuffers");

	CHECK_VKRESULT(
		vulkan_create_command_buffers(context),
		"Failed to create Vulkan command buffers");

	// Create sync objects.
	context->image_available_semaphores = darray_reserve(VkSemaphore, backend->config.frames_in_flight, MEMORY_TAG_RENDERER);
	context->queue_complete_semaphores = darray_reserve(VkSemaphore, backend->config.frames_in_flight, MEMORY_TAG_RENDERER);
	context->in_flight_fences = darray_reserve(vulkan_fence, backend->config.frames_in_flight, MEMORY_TAG_RENDERER);

	for (u8 i = 0; i < backend->config.frames_in_flight; ++i) {
		VkSemaphoreCreateInfo semaphore_create_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		vkCreateSemaphore(context->device.logical_device, &semaphore_create_info, context->allocator, &context->image_available_semaphores[i]);
		vkCreateSemaphore(context->device.logical_device, &semaphore_create_info, context->allocator, &context->queue_complete_semaphores[i]);

		// Create the fence in a signaled state, indicating that the first frame has already been "rendered".
		// This will prevent the application from waiting indefinitely for the first frame to render since it
		// cannot be rendered until a frame is "rendered" before it.
		vulkan_fence_create(context, TRUE, &context->in_flight_fences[i]);
	}

	// In flight fences should not yet exist at this point, so clear the list. These are stored in pointers
	// because the initial state should be 0, and will be 0 when not in use. Acutal fences are not owned
	// by this list.
	context->images_in_flight = darray_reserve(vulkan_fence, context->swapchain.image_count, MEMORY_TAG_RENDERER);

	BX_INFO("Vulkan renderer initialized successfully.");
	return TRUE;
}

void vulkan_renderer_backend_shutdown(box_renderer_backend* backend) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;
	if (context->device.logical_device) vkDeviceWaitIdle(context->device.logical_device);

	// Destroy in the opposite order of creation.

	// Sync objects
	for (u8 i = 0; i < backend->config.frames_in_flight; ++i) {
		vulkan_fence_destroy(context, &context->in_flight_fences[i]);

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
	}

	// Command buffers
	for (u32 i = 0; i < context->swapchain.image_count; ++i) {
		vulkan_command_buffer_free(
			context,
			context->device.graphics_command_pool,
			&context->graphics_command_buffers[i]);

		vulkan_framebuffer_destroy(context, &context->swapchain.framebuffers[i]);
	}

	darray_destroy(context->image_available_semaphores);
	darray_destroy(context->queue_complete_semaphores);
	darray_destroy(context->in_flight_fences);
	darray_destroy(context->images_in_flight);
	darray_destroy(context->graphics_command_buffers);
	darray_destroy(context->swapchain.framebuffers);

	vulkan_renderpass_destroy(context, &context->main_renderpass);

	vulkan_swapchain_destroy(context, &context->swapchain);

	vulkan_device_destroy(context);

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

	vulkan_fence_wait(context, &context->in_flight_fences[context->current_frame], delta_time * 10.0f);
	vulkan_fence_reset(context, &context->in_flight_fences[context->current_frame]);

	// Acquire next swapchain image
	if (!vulkan_swapchain_acquire_next_image_index(
		context, &context->swapchain, UINT64_MAX,
		context->image_available_semaphores[context->current_frame],
		0, &context->image_index)) {
		return FALSE;
	}

	// If this swapchain image is still in flight, wait for it
	if (context->images_in_flight[context->image_index] != VK_NULL_HANDLE) {
		vulkan_fence_wait(context, context->images_in_flight[context->image_index], UINT64_MAX);
	}
	context->images_in_flight[context->image_index] = &context->in_flight_fences[context->current_frame];

	vulkan_command_buffer* cmd = &context->graphics_command_buffers[context->current_frame];
	vulkan_command_buffer_reset(cmd);
	vulkan_command_buffer_begin(cmd, FALSE, FALSE, FALSE);

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

	vkCmdSetViewport(cmd->handle, 0, 1, &viewport);
	vkCmdSetScissor(cmd->handle, 0, 1, &scissor);
	
	context->main_renderpass.size = context->framebuffer_size;
	return TRUE;
}

// TODO: Validate command buffer (box_rendercmd) and put it 
//       in render thread so Vulkan doesn't have too.
b8 vulkan_renderer_playback_rendercmd(box_renderer_backend* backend, box_rendercmd* rendercmd) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;
	if ((backend->config.modes & rendercmd->required_modes) != rendercmd->required_modes) {
		BX_ERROR("Did not configure renderer with selected mode on render command.");
		return FALSE;
	}

	vulkan_command_buffer* cmd = &context->graphics_command_buffers[context->current_frame];

	vulkan_renderpass* current_renderpass = &context->main_renderpass;
	vulkan_framebuffer* current_framebuffer = &context->swapchain.framebuffers[context->image_index];
	box_renderstage* current_shader = NULL;

	u8* cursor = 0;
	while (freelist_next_block(&rendercmd->buffer, &cursor)) {
		rendercmd_header* hdr = (rendercmd_header*)cursor;
		rendercmd_payload* payload = (rendercmd_payload*)(cursor + sizeof(rendercmd_header));

		switch (hdr->type) {
		case RENDERCMD_SET_CLEAR_COLOUR:
			current_renderpass->clear_colour = payload->set_clear_colour.clear_colour;
			vulkan_renderpass_begin(cmd, current_renderpass, current_framebuffer);
			break;

		case RENDERCMD_BEGIN_RENDERSTAGE:
			current_shader = payload->begin_renderstage.renderstage;
			vulkan_graphics_pipeline_bind(cmd, (vulkan_graphics_pipeline*)current_shader->internal_data);
			break;
		
		case RENDERCMD_END_RENDERSTAGE:
			current_shader = NULL;
			break;
		
		case RENDERCMD_BIND_BUFFER:
			box_renderbuffer* buf = payload->bind_buffer.renderbuffer;
			VkDeviceSize offsets[] = { 0 };
			
			vkCmdBindVertexBuffers(cmd->handle, 
				payload->bind_buffer.binding, 1, 
				&((vulkan_buffer*)buf->internal_data)->handle, 
				offsets);
			break;

		case RENDERCMD_DRAW:
			vkCmdDraw(cmd->handle,
				payload->draw.vertex_count,
				payload->draw.instance_count,
				0, 0);
			break;
		}
	}

	if (current_renderpass != NULL &&
		current_renderpass == &context->main_renderpass) {
		vulkan_renderpass_end(cmd, current_renderpass);
	}

	return TRUE; // NOTE: Properaly doesn't need bool (vkCmdXXX doesn't has result either).
}

b8 vulkan_renderer_backend_end_frame(box_renderer_backend* backend) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;
	vulkan_command_buffer* cmd = &context->graphics_command_buffers[context->current_frame];

	vulkan_command_buffer_end(cmd);

	// Submit into queues
	VkSemaphore wait_semaphores[] = { context->image_available_semaphores[context->current_frame] };
	VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSemaphore signal_semaphores[] = { context->queue_complete_semaphores[context->current_frame] };

	VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit_info.waitSemaphoreCount = BX_ARRAYSIZE(wait_semaphores);
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmd->handle;
	submit_info.signalSemaphoreCount = BX_ARRAYSIZE(signal_semaphores);
	submit_info.pSignalSemaphores = signal_semaphores;

	if (!vulkan_result_is_success(
		vkQueueSubmit(context->device.graphics_queue, 1, &submit_info,
		context->in_flight_fences[context->current_frame].handle))) {
		BX_ERROR("vkQueueSubmit failed execution.");
		return FALSE;
	}

	vulkan_command_buffer_update_submitted(cmd);

	// Present
	vulkan_swapchain_present(
		context, &context->swapchain,
		context->device.graphics_queue, context->device.present_queue,
		context->queue_complete_semaphores[context->current_frame],
		context->image_index);

	// Advance frame index
 	context->current_frame = (context->current_frame + 1) % backend->config.frames_in_flight;
	return TRUE;
}

b8 vulkan_renderer_create_renderstage(box_renderer_backend* backend, box_renderstage* out_stage) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;

	out_stage->internal_data = ballocate(sizeof(vulkan_graphics_pipeline), MEMORY_TAG_RENDERER);
	return vulkan_graphics_pipeline_create(context, out_stage->internal_data, &context->main_renderpass, out_stage);
}

void vulkan_renderer_destroy_renderstage(box_renderer_backend* backend, box_renderstage* out_stage) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;

	if (out_stage->internal_data != NULL) {
		vulkan_graphics_pipeline* pipeline = (vulkan_graphics_pipeline*)out_stage->internal_data;
		vulkan_graphics_pipeline_destroy(context, pipeline);
 		bfree(pipeline, sizeof(vulkan_graphics_pipeline), MEMORY_TAG_RENDERER);
	}
}

b8 vulkan_renderer_create_renderbuffer(box_renderer_backend* backend, box_renderbuffer* out_buffer) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;
	if (!(backend->config.modes & RENDERER_MODE_TRANSFER)) {
		BX_WARN("Attempting to transfer data between CPU and GPU without enabling, falling back to graphics queue.");
	}

	// TODO: Make configurable
	VkBufferUsageFlagBits buffer_usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

	out_buffer->internal_data = ballocate(sizeof(vulkan_buffer), MEMORY_TAG_RENDERER);
	vulkan_buffer* buffer = (vulkan_buffer*)out_buffer->internal_data;
	
	vulkan_buffer staging_buffer = { 0 };
	CHECK_VKRESULT(
		vulkan_buffer_create(
			context,
			out_buffer->temp_user_size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&staging_buffer),
		"Failed to create Vulkan staging buffer");

	vulkan_buffer_map_data(context, &staging_buffer, out_buffer->temp_user_size, out_buffer->temp_user_data);

	CHECK_VKRESULT(
		vulkan_buffer_create(
			context,
			out_buffer->temp_user_size,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | buffer_usage,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			buffer),
		"Failed to create internal Vulkan renderbuffer");

	vulkan_command_buffer command_buffer;
	vulkan_command_buffer_allocate_and_begin_single_use(context, context->device.graphics_command_pool, &command_buffer);
	
	VkBufferCopy copy_info = { 0 };
	copy_info.size = out_buffer->temp_user_size;
	vkCmdCopyBuffer(command_buffer.handle, staging_buffer.handle, buffer->handle, 1, &copy_info);

	CHECK_VKRESULT(
		vulkan_command_buffer_end_single_use(
			context,
			context->device.graphics_command_pool,
			&command_buffer,
			backend->config.modes & RENDERER_MODE_TRANSFER ?
				context->device.transfer_queue :
				context->device.graphics_queue),
		"Failed to transfer Vulkan renderbuffer to GPU"
	);
	
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

VkResult vulkan_regenerate_framebuffer(vulkan_context* context) {
	for (u32 i = 0; i < context->swapchain.image_count; ++i) {
		// TODO: make this dynamic based on the currently configured attachments
		VkImageView attachments[] = { context->swapchain.views[i] };

		VkResult result = vulkan_framebuffer_create(
			context,
			&context->main_renderpass,
			context->framebuffer_size.width,
			context->framebuffer_size.height,
			BX_ARRAYSIZE(attachments),
			attachments,
			&context->swapchain.framebuffers[i]);
		if (!vulkan_result_is_success(result)) return result;
	}

	return VK_SUCCESS;
}

VkResult vulkan_create_command_buffers(vulkan_context* context) {
	if (!context->graphics_command_buffers) {
		context->graphics_command_buffers = darray_reserve(vulkan_command_buffer, context->swapchain.image_count, MEMORY_TAG_RENDERER);
	}

	for (u32 i = 0; i < context->swapchain.image_count; ++i) {
		if (context->graphics_command_buffers[i].handle) {
			vulkan_command_buffer_free(
				context,
				context->device.graphics_command_pool,
				&context->graphics_command_buffers[i]);
		}

		bzero_memory(&context->graphics_command_buffers[i], sizeof(vulkan_command_buffer));

		VkResult result = vulkan_command_buffer_allocate(
			context,
			context->device.graphics_command_pool,
			TRUE,
			&context->graphics_command_buffers[i]);
		if (!vulkan_result_is_success(result)) return result;
	}

	BX_TRACE("Vulkan command buffers created.");
	return VK_SUCCESS;
}