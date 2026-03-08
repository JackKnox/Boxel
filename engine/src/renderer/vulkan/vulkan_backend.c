#include "defines.h"
#include "vulkan_backend.h"

#include "platform/filesystem.h"

#include "utils/darray.h"

#include "vulkan_types.h"

#include "vulkan_command_buffer.h"
#include "vulkan_device.h"
#include "vulkan_renderbuffer.h"
#include "vulkan_renderstage.h"
#include "vulkan_rendertarget.h"
#include "vulkan_texture.h"
#include "vulkan_window_system.h"

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
	context->config = *config; // TODO: Remove
	context->allocator = 0; // TODO: Custom allocator

	VkApplicationInfo app_info = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app_info.apiVersion = VK_API_VERSION_1_2;
	app_info.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
	app_info.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
	app_info.pApplicationName = application_name;
	app_info.pEngineName = "Boxel";

	const char** platform_extensions = NULL;
	u32 platform_extensions_count = vulkan_platform_get_required_extensions(&platform_extensions);

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

	CHECK_VKRESULT(
		vkCreateInstance(&create_info, context->allocator, &context->instance), 
		"Failed to create Vulkan instance");

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

		PFN_vkCreateDebugUtilsMessengerEXT func = 
			(PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context->instance, "vkCreateDebugUtilsMessengerEXT");

		CHECK_VKRESULT(
			func(context->instance, &debug_create_info, context->allocator, &context->debug_messenger),
			"Failed to create Vulkan debug messanger");
	}

	// Create the Vulkan device
	CHECK_VKRESULT(
		vulkan_device_create(backend),
		"Failed to create Vulkan device");

	if (backend->plat_state != NULL) {
		backend->plat_state->internal_renderer_state = ballocate(sizeof(vulkan_window_system), MEMORY_TAG_RENDERER);
		vulkan_window_system* window_system = (vulkan_window_system*)backend->plat_state->internal_renderer_state;

		if (!vulkan_window_system_create(context, backend->plat_state, starting_size, window_system)) {
			BX_ERROR("Failed to connect Vulkan to platform surface");
			return FALSE;
		}
	}

	context->in_flight_fences = darray_reserve(VkFence, context->config.frames_in_flight, MEMORY_TAG_RENDERER);

	VkFenceCreateInfo fence_create_info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	for (u32 i = 0; i < context->config.frames_in_flight; ++i) {
		VkFence* fence = darray_push_empty(context->in_flight_fences);

		CHECK_VKRESULT(
			vkCreateFence(
				context->device.logical_device,
				&fence_create_info,
				context->allocator,
				fence),
			"Failed to create Vulkan sync objects");
	}

	if (context->config.modes & RENDERER_MODE_GRAPHICS) {
		vulkan_queue_type queue_type = VULKAN_QUEUE_TYPE_GRAPHICS;
		context->command_buffer_ring[queue_type] = darray_reserve(vulkan_command_buffer, context->config.frames_in_flight, MEMORY_TAG_RENDERER);

		for (u32 i = 0; i < context->config.frames_in_flight; ++i) {
			// TODO: Allocate command buffer all at once.
			CHECK_VKRESULT(
				vulkan_command_buffer_allocate(
					context,
					&context->device.mode_queues[queue_type],
					TRUE,
					&context->command_buffer_ring[queue_type][i]),
				"Failed to create Vulkan command buffers");

		}
	}

	if (context->config.modes & RENDERER_MODE_COMPUTE) {
		vulkan_queue_type queue_type = VULKAN_QUEUE_TYPE_COMPUTE;
		context->command_buffer_ring[queue_type] = darray_reserve(vulkan_command_buffer, context->config.frames_in_flight, MEMORY_TAG_RENDERER);

		for (u32 i = 0; i < context->config.frames_in_flight; ++i) {
			// TODO: Allocate command buffer all at once.
			CHECK_VKRESULT(
				vulkan_command_buffer_allocate(
					context,
					&context->device.mode_queues[queue_type],
					TRUE,
					&context->command_buffer_ring[queue_type][i]),
				"Failed to create Vulkan command buffers");
		}
	}

	context->memory_barriers = darray_create(memory_barrier, MEMORY_TAG_RENDERER);
	context->queued_submissions = darray_create(vulkan_queue_submission, MEMORY_TAG_RENDERER);
	context->semaphore_pool = darray_create(VkSemaphore, MEMORY_TAG_RENDERER);

	VkSemaphoreCreateInfo semaphore_create_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	for (u32 i = 0; i < VULKAN_QUEUE_TYPE_MAX * context->config.frames_in_flight; ++i) {
		VkSemaphore* semaphore = darray_push_empty(context->semaphore_pool);

		CHECK_VKRESULT(
			vkCreateSemaphore(
				context->device.logical_device,
				&semaphore_create_info,
				context->allocator,
				semaphore),
			"Failed to create Vulkan semaphore in pool");
	}
	return TRUE;
}

void vulkan_renderer_backend_shutdown(box_renderer_backend* backend) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;

	// Destroy in the opposite order of creation.

	if (context->semaphore_pool != NULL) {
		for (u32 i = 0; i < darray_length(context->semaphore_pool); ++i) {
			if (!context->semaphore_pool[i]) continue;

			vkDestroySemaphore(
				context->device.logical_device, 
				context->semaphore_pool[i], 
				context->allocator);
		}

		darray_destroy(context->semaphore_pool);
	}

	if (context->memory_barriers != NULL) darray_destroy(context->memory_barriers);
	if (context->queued_submissions != NULL) darray_destroy(context->queued_submissions);

	for (u32 i = 0; i < BX_ARRAYSIZE(context->command_buffer_ring); ++i) {
		if (!context->command_buffer_ring[i]) continue;

		for (u32 j = 0; j < darray_length(context->command_buffer_ring[i]); ++j) {
			vulkan_command_buffer_free(
				context,
				&context->command_buffer_ring[i][j]);
		}

		darray_destroy(context->command_buffer_ring[i]);
	}

	if (context->in_flight_fences != NULL) {
		for (u32 i = 0; i < darray_length(context->in_flight_fences); ++i) {
			if (!context->in_flight_fences[i]) continue;

			vkDestroyFence(
				context->device.logical_device,
				context->in_flight_fences[i],
				context->allocator);
		}

		darray_destroy(context->in_flight_fences);
	}

	if (backend->plat_state->internal_renderer_state != NULL) {
		vulkan_window_system* window_system = (vulkan_window_system*)backend->plat_state->internal_renderer_state;

		vulkan_window_system_destroy(context, window_system);
		bfree(window_system, sizeof(vulkan_window_system), MEMORY_TAG_RENDERER);
		backend->plat_state->internal_renderer_state = NULL;
	}

	vulkan_device_destroy(backend);

	if (context->instance) {
		if (context->debug_messenger) {
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

b8 vulkan_renderer_backend_create_rendertarget_on_platform(box_renderer_backend* backend, box_rendertarget** out_rendertarget) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;
	
	if (backend->plat_state == NULL) {
		BX_ERROR("Renderer backend isn't connected to a platform surface");
		return FALSE;
	}
	
	vulkan_window_system* window_system = (vulkan_window_system*)backend->plat_state->internal_renderer_state;
    return vulkan_window_system_connect_rendertarget(context, window_system, out_rendertarget);
}

void vulkan_renderer_backend_on_resized(box_renderer_backend* backend, uvec2 new_size) {
	// TODO: Swapchain recreation.
}

b8 vulkan_renderer_backend_begin_frame(box_renderer_backend* backend, f64 delta_time) {
    vulkan_context* context = (vulkan_context*)backend->internal_context;

	CHECK_VKRESULT(
		vkWaitForFences(
			context->device.logical_device,
			1, &context->in_flight_fences[context->current_frame],
			TRUE,
			UINT64_MAX),
		"Failed to wait or reset internal Vulkan fence");

	CHECK_VKRESULT(
		vkResetFences(
			context->device.logical_device, 
			1, &context->in_flight_fences[context->current_frame]),
		"Failed to wait or reset Vulkan fence");
	
	if (backend->plat_state != NULL) {
		vulkan_window_system* window_system = (vulkan_window_system*)backend->plat_state->internal_renderer_state;
		
		if (!vulkan_window_system_acquire_frame(
			context, 
			window_system,
			delta_time, 
			UINT64_MAX)) {
			BX_ERROR("Failed to acquire next window frame in Vulkan renderer");
			return FALSE;
		}
	}

	darray_length_set(context->memory_barriers, 0);
	darray_length_set(context->queued_submissions, 0);
	context->last_mode = 0;
    return TRUE;
}

vulkan_queue_type box_renderer_mode_to_queue_type(box_renderer_mode mode) {
	switch (mode) {
	case RENDERER_MODE_GRAPHICS: return VULKAN_QUEUE_TYPE_GRAPHICS;
	case RENDERER_MODE_COMPUTE: return VULKAN_QUEUE_TYPE_COMPUTE;
	case RENDERER_MODE_TRANSFER: return VULKAN_QUEUE_TYPE_TRANSFER;

	default:
		BX_ASSERT(FALSE && "Unsupported renderer mode!");
		break;
	}

	return 0;
}

void vulkan_renderer_execute_command(box_renderer_backend* backend, box_rendercmd_context* rendercmd_context, rendercmd_header* header, rendercmd_payload* payload) {
    vulkan_context* context = (vulkan_context*)backend->internal_context;

	if (rendercmd_context->current_mode != context->last_mode) {
		// Create new queue submission
		vulkan_queue_submission* new = darray_push_empty(context->queued_submissions);

		new->command_buffer = &context->command_buffer_ring
			[box_renderer_mode_to_queue_type(rendercmd_context->current_mode)]
			[context->current_frame];

		new->signal_semaphore = context->semaphore_pool[context->semaphore_next_index];
		context->semaphore_next_index = (context->semaphore_next_index + 1) % darray_length(context->semaphore_pool);

		new->wait_semaphores = darray_create(VkSemaphore, MEMORY_TAG_RENDERER);

        vulkan_command_buffer_reset(new->command_buffer);
		vulkan_command_buffer_begin(new->command_buffer, FALSE, FALSE, FALSE);
	}

	context->last_mode = rendercmd_context->current_mode;
	
	vulkan_queue_submission* curr_submission = &context->queued_submissions[darray_length(context->queued_submissions) - 1];

    switch (header->type) {
    case RENDERCMD_BIND_RENDERTARGET:
        vulkan_rendertarget_begin(
			context, curr_submission->command_buffer,
            rendercmd_context->current_target,
            TRUE, TRUE);
        break;

	case RENDERCMD_MEMORY_BARRIER:
		memory_barrier* barrier = darray_push_empty(context->memory_barriers);
		barrier->created_on_submission = darray_length(context->queued_submissions) - 1;
		barrier->src_renderstage = payload->memory_barrier.src_renderstage;
		barrier->dst_renderstage = payload->memory_barrier.dst_renderstage;
		barrier->src_access = payload->memory_barrier.src_access;
		barrier->dst_access = payload->memory_barrier.dst_access;
		break;

    case RENDERCMD_BEGIN_RENDERSTAGE:
		for (u32 i = 0; i < darray_length(context->memory_barriers); ++i) {
			if (context->memory_barriers[i].dst_renderstage != payload->begin_renderstage.renderstage)
				continue;
			
			memory_barrier barrier = {};
			darray_pop_at(context->memory_barriers, i, &barrier);

			darray_push(curr_submission->wait_semaphores, 
						context->queued_submissions[barrier.created_on_submission].signal_semaphore);
			--i;
		}

        vulkan_renderstage_bind(
            context, curr_submission->command_buffer,
            rendercmd_context->current_shader);
        break;

    case RENDERCMD_DRAW:
        vkCmdDraw(curr_submission->command_buffer->handle,
                  payload->draw.vertex_count,
                  payload->draw.instance_count,
                  0, 0);
        break;

    case RENDERCMD_DRAW_INDEXED:
        vkCmdDrawIndexed(curr_submission->command_buffer->handle,
                         payload->draw_indexed.index_count,
                         payload->draw_indexed.instance_count,
                         0, 0, 0);
        break;

    case RENDERCMD_DISPATCH:
        vkCmdDispatch(curr_submission->command_buffer->handle,
                      payload->dispatch.group_size.x,
                      payload->dispatch.group_size.y,
                      payload->dispatch.group_size.z);
        break;

    case RENDERCMD_END:
        if (rendercmd_context->current_target)
            vulkan_rendertarget_end(
                context, curr_submission->command_buffer,
                rendercmd_context->current_target);
        break;
    }
}

b8 vulkan_renderer_backend_end_frame(box_renderer_backend* backend) {
    vulkan_context* context = (vulkan_context*)backend->internal_context;

	VkSemaphore render_complete_semaphore;

	for (u32 i = 0; i < darray_length(context->queued_submissions); ++i) {
		vulkan_queue_submission* submission = &context->queued_submissions[i];
		VkFence signal_fence = VK_NULL_HANDLE;

		vulkan_command_buffer_end(submission->command_buffer);
		// TODO: Propbably change this at some point...
		VkPipelineStageFlags wait_flags[] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };

		if (i == darray_length(context->queued_submissions) - 1)
			signal_fence = context->in_flight_fences[context->current_frame];
		
		CHECK_VKRESULT(
			vulkan_command_buffer_submit(
				submission->command_buffer,
				darray_length(submission->wait_semaphores), submission->wait_semaphores, 
				1, &submission->signal_semaphore,
				wait_flags,
				signal_fence),
			"Failed to submit Vulkan command buffer");
		
		render_complete_semaphore = submission->signal_semaphore;
		darray_destroy(submission->wait_semaphores);
	}

	if (backend->plat_state != NULL) {
		vulkan_window_system* window_system = (vulkan_window_system*)backend->plat_state->internal_renderer_state;
		
		if (!vulkan_window_system_present(
			context, 
			window_system, 
			context->device.mode_queues[VULKAN_QUEUE_TYPE_PRESENT].handle, 
			&render_complete_semaphore, 1)) {
			BX_ERROR("Failed to present internal Vulkan swapchain");
			return FALSE;
		}
	}

	// Advance to next frame
    context->current_frame = (context->current_frame + 1) % context->config.frames_in_flight;
    return TRUE;
}