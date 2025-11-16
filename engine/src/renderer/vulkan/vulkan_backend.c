#include "defines.h"
#include "vulkan_backend.h"

#include "engine.h"
#include "platform/filesystem.h"
#include "renderer/renderer_cmd.h"

#include "vulkan_types.h"
#include "vulkan_device.h"
#include "vulkan_swapchain.h"
#include "vulkan_renderpass.h"
#include "vulkan_pipeline.h"
#include "vulkan_framebuffer.h"
#include "vulkan_command_buffer.h"
#include "vulkan_fence.h"

b8 platform_create_vulkan_surface(VkInstance instance, box_platform* plat_state, const struct VkAllocationCallbacks* allocator, VkSurfaceKHR* out_surface);

VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
	VkDebugUtilsMessageTypeFlagsEXT message_types,
	const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
	void* user_data) {
	switch (message_severity) {
	default:
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		BX_ERROR(callback_data->pMessage);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		BX_WARN(callback_data->pMessage);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		BX_INFO(callback_data->pMessage);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
		BX_TRACE(callback_data->pMessage);
		break;
	}
	return VK_FALSE;
}

b8 vulkan_renderer_backend_initialize(renderer_backend* backend, const char* application_name, renderer_backend_config* config) {
	backend->internal_context = platform_allocate(sizeof(vulkan_context), FALSE);
	platform_zero_memory(backend->internal_context, sizeof(vulkan_context));

	vulkan_context* context = (vulkan_context*)backend->internal_context;
	context->config = config;
	context->allocator = 0;
	context->current_frame = 0;
	
	VkApplicationInfo app_info = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app_info.apiVersion = VK_API_VERSION_1_2;
	app_info.pApplicationName = application_name;
	app_info.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
	app_info.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
	app_info.pEngineName = "Boxel";

	VkInstanceCreateInfo create_info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	create_info.pApplicationInfo = &app_info;

	// Obtain a list of required extensions
	const char** required_extensions = darray_create(const char*);
	const char** required_validation_layer_names = darray_create(const char*);

	// Generic surface extension
	darray_push(required_extensions, VK_KHR_SURFACE_EXTENSION_NAME);

	// Platform-specific extension(s)
	platform_get_vulkan_extensions(&required_extensions);

	// Add debug extensions/layers if enabled
	if (context->config->enable_validation) {
		darray_push(required_extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		darray_push(required_validation_layer_names, "VK_LAYER_KHRONOS_validation");
	}

	// Fill create info
	create_info.enabledExtensionCount = darray_length(required_extensions);
	create_info.ppEnabledExtensionNames = required_extensions;
	create_info.enabledLayerCount = darray_length(required_validation_layer_names);
	create_info.ppEnabledLayerNames = required_validation_layer_names;

	BX_TRACE("Required extensions:");
	for (u32 i = 0; i < darray_length(required_extensions); ++i) {
		BX_TRACE("    %s", required_extensions[i]);
	}

	if (vkCreateInstance(&create_info, context->allocator, &context->instance) != VK_SUCCESS) {
		BX_ERROR("Failed to create Vulkan instance!");
		return FALSE;
	}

	BX_INFO("Vulkan Instance created.");

	// Clean up temp arrays
	darray_destroy(required_extensions);
	darray_destroy(required_validation_layer_names);

	// Setup debug messenger if validation enabled
	if (context->config->enable_validation) {
		u32 log_severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;

		VkDebugUtilsMessengerCreateInfoEXT debug_create_info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
		debug_create_info.messageSeverity = log_severity;
		debug_create_info.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
		debug_create_info.pfnUserCallback = vk_debug_callback;

		PFN_vkCreateDebugUtilsMessengerEXT func =
			(PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context->instance, "vkCreateDebugUtilsMessengerEXT");

		VK_CHECK(func(context->instance, &debug_create_info, context->allocator, &context->debug_messenger));
		BX_INFO("Vulkan debugger created.");
	}

	// Create the Vulkan surface
	if (!platform_create_vulkan_surface(context->instance, backend->plat_state, context->allocator, &context->surface)) {
		BX_ERROR("Failed to create platform surface!");
		return FALSE;
	}
	BX_INFO("Vulkan surface created.");

	if (!vulkan_device_create(context)) {
		BX_ERROR("Failed to create device!");
		return FALSE;
	}

	if (!vulkan_device_detect_depth_format(&context->device)) {
		context->device.depth_format = VK_FORMAT_UNDEFINED;
		BX_ERROR("Failed to find a depth supported format!");
	}

	if (!vulkan_swapchain_create(context, context->config->framebuffer_width, context->config->framebuffer_height, &context->swapchain)) {
		BX_ERROR("Failed to create swapchain!");
		return FALSE;
	}

	if (!vulkan_renderpass_create(context, &context->main_renderpass, 0, 0,
		context->config->framebuffer_width, context->config->framebuffer_height,
		1.0f, 0)) {
		BX_ERROR("Failed to create main renderpass!");
		return FALSE;
	}

	const char* graphics_shaders[] = { "assets/shader_base.vert.spv", "assets/shader_base.frag.spv" };

	if (!vulkan_graphics_pipeline_create(context, &context->graphics_pipeline, 
		&context->main_renderpass, graphics_shaders, BX_ARRAYSIZE(graphics_shaders))) {
		BX_ERROR("Failed to create graphics pipeline!");
		return FALSE;
	}

	context->swapchain.framebuffers = darray_reserve(vulkan_framebuffer, context->swapchain.image_count);
	vulkan_regenerate_framebuffer(backend);
	vulkan_create_command_buffers(backend);

	// Create sync objects.
	context->image_available_semaphores = darray_reserve(VkSemaphore, context->swapchain.max_frames_in_flight);
	context->queue_complete_semaphores = darray_reserve(VkSemaphore, context->swapchain.max_frames_in_flight);
	context->in_flight_fences = darray_reserve(vulkan_fence, context->swapchain.max_frames_in_flight);

	for (u8 i = 0; i < context->swapchain.max_frames_in_flight; ++i) {
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
	context->images_in_flight = darray_reserve(vulkan_fence, context->swapchain.image_count);

	BX_INFO("Vulkan renderer initialized successfully.");
	return TRUE;
}

void vulkan_renderer_backend_shutdown(renderer_backend* backend) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;
	vkDeviceWaitIdle(context->device.logical_device);

	// Destroy in the opposite order of creation.

	vulkan_graphics_pipeline_destroy(context, &context->graphics_pipeline);

	// Sync objects
	for (u8 i = 0; i < context->swapchain.max_frames_in_flight; ++i) {
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

	platform_free(context, FALSE);
	backend->internal_context = NULL;
}

void vulkan_renderer_backend_on_resized(renderer_backend* backend, u32 width, u32 height) {
}

b8 vulkan_renderer_backend_begin_frame(renderer_backend* backend, box_rendercmd* frame_cmd, f32 delta_time) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;

	// Wait for this frame's fence (NOT image index)
	vulkan_fence_wait(context, &context->in_flight_fences[context->current_frame], UINT64_MAX);
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

	// Reset the command buffer for this *frame* (NOT image index)
	vulkan_command_buffer* cmd = &context->graphics_command_buffers[context->current_frame];
	vulkan_command_buffer_reset(cmd);
	vulkan_command_buffer_begin(cmd, FALSE, FALSE, FALSE);

	// Set dynamic state, begin render pass, etc.
	VkViewport viewport;
	viewport.x = 0.0f;
	viewport.y = (f32)context->config->framebuffer_height;
	viewport.width = (f32)context->config->framebuffer_width;
	viewport.height = -(f32)context->config->framebuffer_height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	// Scissor
	VkRect2D scissor;
	scissor.offset.x = scissor.offset.y = 0;
	scissor.extent.width = context->config->framebuffer_width;
	scissor.extent.height = context->config->framebuffer_height;

	vkCmdSetViewport(cmd->handle, 0, 1, &viewport);
	vkCmdSetScissor(cmd->handle, 0, 1, &scissor);
	
	context->main_renderpass.w = context->config->framebuffer_width;
	context->main_renderpass.h = context->config->framebuffer_height;
	return TRUE;
}

b8 vulkan_renderer_backend_end_frame(renderer_backend* backend) {
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

	VK_CHECK(vkQueueSubmit(context->device.graphics_queue, 1, &submit_info,
		context->in_flight_fences[context->current_frame].handle));

	// Present
	vulkan_swapchain_present(
		context, &context->swapchain,
		context->device.graphics_queue, context->device.present_queue,
		context->queue_complete_semaphores[context->current_frame],
		context->image_index);

	// Advance frame index
	context->current_frame = (context->current_frame + 1) % context->swapchain.max_frames_in_flight;
	return TRUE;
}

b8 vulkan_regenerate_framebuffer(renderer_backend* backend) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;

	for (u32 i = 0; i < context->swapchain.image_count; ++i) {
		// TODO: make this dynamic based on the currently configured attachments
		u32 attachment_count = 2;
		VkImageView attachments[] = {
			context->swapchain.views[i],
			context->swapchain.depth_attachment.view };

		vulkan_framebuffer_create(
			context,
			&context->main_renderpass,
			context->config->framebuffer_width,
			context->config->framebuffer_height,
			attachment_count,
			attachments,
			&context->swapchain.framebuffers[i]);
	}

	return TRUE;
}

b8 vulkan_create_command_buffers(renderer_backend* backend) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;

	if (!context->graphics_command_buffers) {
		context->graphics_command_buffers = darray_reserve(vulkan_command_buffer, context->swapchain.image_count);
	}

	for (u32 i = 0; i < context->swapchain.image_count; ++i) {
		if (context->graphics_command_buffers[i].handle) {
			vulkan_command_buffer_free(
				context,
				context->device.graphics_command_pool,
				&context->graphics_command_buffers[i]);
		}

		platform_zero_memory(&context->graphics_command_buffers[i], sizeof(vulkan_command_buffer));

		vulkan_command_buffer_allocate(
			context,
			context->device.graphics_command_pool,
			TRUE,
			&context->graphics_command_buffers[i]);
	}

	BX_TRACE("Vulkan command buffers created.");
	return TRUE;
}
