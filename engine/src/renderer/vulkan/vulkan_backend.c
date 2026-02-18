#include "defines.h"
#include "vulkan_backend.h"

#include "engine.h"
#include "platform/filesystem.h"

#include "utils/darray.h"

#include "vulkan_types.h"

#include "vulkan_command_buffer.h"
#include "vulkan_renderbuffer.h"
#include "vulkan_renderstage.h"
#include "vulkan_rendertarget.h"
#include "vulkan_texture.h"
#include "vulkan_device.h"
#include "vulkan_swapchain.h"
#include "vulkan_fence.h"

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

b8 vulkan_renderer_backend_initialize(box_renderer_backend* backend, box_renderer_backend_config* config, uvec2 starting_size, const char* application_name) {
	backend->internal_context = ballocate(sizeof(vulkan_context), MEMORY_TAG_RENDERER);

	vulkan_context* context = (vulkan_context*)backend->internal_context;
	context->framebuffer_size = (vec2){ starting_size.x, starting_size.y };
	context->config = *config; // TODO: Remove
	context->allocator = 0; // TODO: Custom allocator
	
	VkApplicationInfo app_info = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app_info.apiVersion = VK_API_VERSION_1_2; // TODO: Does it need to be this low??
	app_info.pApplicationName = application_name;
	app_info.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
	app_info.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
	app_info.pEngineName = "Boxel";

	const char** platform_extensions = NULL;
	u32 platform_extensions_count = backend->plat_state->get_required_vulkan_extensions(backend->plat_state, &platform_extensions);

	// Obtain a list of required extensions
	const char** required_extensions = darray_reserve(const char*, platform_extensions_count, MEMORY_TAG_RENDERER);
	const char** required_validation_layer_names = darray_create(const char*, MEMORY_TAG_RENDERER);

	for (int i = 0; i < platform_extensions_count; ++i)
		darray_push(required_extensions, platform_extensions[i]);

	// Add debug extensions/layers if enabled
	if (context->config.enable_validation) {
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
	if (context->config.enable_validation) {
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
		backend->plat_state->create_vulkan_surface(backend->plat_state, context->instance, context->allocator, &context->surface),
		"Failed to create Vulkan platform surface");

	BX_INFO("Vulkan surface created.");

	// Create the Vulkan device
	CHECK_VKRESULT(
		vulkan_device_create(backend),
		"Failed to create Vulkan device");

	BX_INFO("Vulkan device created.");

	// Create the Vulkan swapchain
	CHECK_VKRESULT(
		vulkan_swapchain_create(context, context->framebuffer_size, &context->swapchain),
		"Failed to create Vulkan swapchain");

	// Create intermediate objects.
	context->queue_complete_semaphores = darray_reserve(VkSemaphore, context->swapchain.image_count, MEMORY_TAG_RENDERER);
	context->images_in_flight = darray_reserve(vulkan_fence*, context->swapchain.image_count, MEMORY_TAG_RENDERER);

	context->image_available_semaphores = darray_reserve(VkSemaphore, context->config.frames_in_flight, MEMORY_TAG_RENDERER);
	context->in_flight_fences = darray_reserve(vulkan_fence, context->config.frames_in_flight, MEMORY_TAG_RENDERER);

	// Create framebuffers & command buffers.
	for (u32 i = 0; i < context->swapchain.image_count; ++i) {
		VkSemaphoreCreateInfo semaphore_create_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

		CHECK_VKRESULT(
			vkCreateSemaphore(
				context->device.logical_device, 
				&semaphore_create_info, 
				context->allocator, 
				&context->queue_complete_semaphores[i]),
			"Failed to create Vulkan sync objects");
	}

	for (u32 i = 0; i < context->config.frames_in_flight; ++i) {
		VkSemaphoreCreateInfo semaphore_create_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		
		CHECK_VKRESULT(
			vkCreateSemaphore(
				context->device.logical_device, 
				&semaphore_create_info, 
				context->allocator,
				&context->image_available_semaphores[i]),
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

	BX_TRACE("Vulkan synchronization objects created.");

	if (context->config.modes & RENDERER_MODE_GRAPHICS) {
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

	if (context->config.modes & RENDERER_MODE_COMPUTE) {
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

	BX_INFO("Vulkan renderer initialized successfully.");
	return TRUE;
}

void vulkan_renderer_backend_shutdown(box_renderer_backend* backend) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;

	// Destroy in the opposite order of creation.

	for (u32 i = 0; i < BX_ARRAYSIZE(context->command_buffer_ring); ++i) {
		if (!context->command_buffer_ring[i]) continue;

		for (u32 j = 0; j < darray_length(context->command_buffer_ring[i]); ++j) {
			vulkan_command_buffer_free(
				context,
				&context->command_buffer_ring[i][j]);
		}

		darray_destroy(context->command_buffer_ring[i]);
	}

	// Destroy framebuffers & command buffers.
	for (u32 i = 0; i < context->swapchain.image_count; ++i) {
		if (context->queue_complete_semaphores[i]) {
			vkDestroySemaphore(
				context->device.logical_device,
				context->queue_complete_semaphores[i],
				context->allocator);
		}
	}

	for (u32 i = 0; i < context->config.frames_in_flight; ++i) {
		vulkan_fence_destroy(
			context,
			&context->in_flight_fences[i]);
		
		if (context->image_available_semaphores[i]) {
			vkDestroySemaphore(
				context->device.logical_device,
				context->image_available_semaphores[i],
				context->allocator);
		}
	}

	darray_destroy(context->images_in_flight);
	darray_destroy(context->image_available_semaphores);
	darray_destroy(context->queue_complete_semaphores);
	darray_destroy(context->in_flight_fences);	

	BX_INFO("Destroying Vulkan swapchain...");

	vulkan_swapchain_destroy(context, &context->swapchain);

	vulkan_device_destroy(backend);

	if (context->surface) vkDestroySurfaceKHR(context->instance, context->surface, context->allocator);

	if (context->instance) {
		if (context->config.enable_validation) {
			PFN_vkDestroyDebugUtilsMessengerEXT func =
				(PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context->instance, "vkDestroyDebugUtilsMessengerEXT");
			func(context->instance, context->debug_messenger, context->allocator);
		}

		vkDestroyInstance(context->instance, context->allocator);
	}

	bfree(context, sizeof(vulkan_context), MEMORY_TAG_RENDERER);
	backend->internal_context = NULL;
}

void vulkan_renderer_backend_wait_until_idle(box_renderer_backend* backend, u64 timeout) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;
	if (context->device.logical_device) vkDeviceWaitIdle(context->device.logical_device);
}

void vulkan_renderer_backend_on_resized(box_renderer_backend* backend, uvec2 new_size) {
	// TODO: Swapchain recreation.
}

b8 vulkan_renderer_backend_begin_frame(box_renderer_backend* backend, f32 delta_time) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;

	vulkan_fence_wait(context, &context->in_flight_fences[context->current_frame], UINT64_MAX);
	vulkan_fence_reset(context, &context->in_flight_fences[context->current_frame]);

	if (context->config.modes & RENDERER_MODE_GRAPHICS) {
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

	context->rendered_this_frame = FALSE;
	return TRUE;
}

vulkan_queue_type box_renderer_mode_to_queue_type(box_renderer_mode mode) {
	switch (mode) {
	case RENDERER_MODE_GRAPHICS: return VULKAN_QUEUE_TYPE_GRAPHICS;
	case RENDERER_MODE_COMPUTE: return VULKAN_QUEUE_TYPE_COMPUTE;
	case RENDERER_MODE_TRANSFER: return VULKAN_QUEUE_TYPE_TRANSFER;
	}

	return 0;
}

void vulkan_renderer_execute_command(box_renderer_backend* backend, box_rendercmd_context* rendercmd_context, rendercmd_header* header, rendercmd_payload* payload) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;
	if (header->type != RENDERCMD_END) context->rendered_this_frame = TRUE;

	vulkan_command_buffer* command_buffer = &context->command_buffer_ring[box_renderer_mode_to_queue_type(rendercmd_context->current_mode)][context->current_frame];
	command_buffer->used = TRUE;

	switch (header->type) {
	case RENDERCMD_BIND_RENDERTARGET:
		vulkan_rendertarget_begin(backend, command_buffer, rendercmd_context->current_target, TRUE, TRUE);
		break;

	case RENDERCMD_BEGIN_RENDERSTAGE:
		vulkan_renderstage_bind(backend, command_buffer, rendercmd_context->current_shader);
		break;

	case RENDERCMD_DRAW:
		vkCmdDraw(
			command_buffer->handle,
			payload->draw.vertex_count,
			payload->draw.instance_count,
			0, 0);
		break;

	case RENDERCMD_DRAW_INDEXED:
		vkCmdDrawIndexed(
			command_buffer->handle,
			payload->draw_indexed.index_count,
			payload->draw_indexed.instance_count,
			0, 0, 0);
		break;

	case RENDERCMD_DISPATCH:
		vkCmdDispatch(
			command_buffer->handle,
			payload->dispatch.group_size.x,
			payload->dispatch.group_size.y,
			payload->dispatch.group_size.z);
		break;

	case RENDERCMD_END:
		if (rendercmd_context->current_target != NULL)
			vulkan_rendertarget_end(backend, command_buffer, rendercmd_context->current_target);
		break;
	}
}

b8 vulkan_renderer_backend_end_frame(box_renderer_backend* backend) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;

	for (u32 i = 0; i < BX_ARRAYSIZE(context->command_buffer_ring); ++i) {
		if (!context->command_buffer_ring[i]) continue;
		vulkan_command_buffer* cmd = &context->command_buffer_ring[i][context->current_frame];

		b8 used_command_buffer = cmd->used;
		vulkan_command_buffer_end(cmd);

		if (!used_command_buffer) continue;

		// Submit into queues
		VkSemaphore wait_semaphores[] = { context->image_available_semaphores[context->current_frame] };
		VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };
		VkSemaphore signal_semaphores[] = { context->queue_complete_semaphores[context->image_index] };

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

	if (context->rendered_this_frame && context->config.modes & RENDERER_MODE_GRAPHICS) {
		// Present
		CHECK_VKRESULT(
			vulkan_swapchain_present(
				context,
				&context->swapchain,
				context->device.mode_queues[VULKAN_QUEUE_TYPE_PRESENT].handle,
				context->queue_complete_semaphores[context->image_index],
				context->image_index),
			"Failed to present Vulkan swapchain");
	}

	if (!context->rendered_this_frame) {
		BX_FATAL("Didn't render anything on backend but still called begin/end frame.");
		return FALSE;
	}

	// Advance frame index
	context->current_frame = (context->current_frame + 1) % context->config.frames_in_flight;
	return TRUE;
}