#include "defines.h"
#include "vulkan_backend.h"

#include "platform/filesystem.h"

#include "utils/darray.h"
#include "utils/string_utils.h"

#include "vulkan_types.h"

#include "vulkan_command_buffer.h"
#include "vulkan_device.h"
#include "vulkan_renderbuffer.h"
#include "vulkan_renderstage.h"
#include "vulkan_rendertarget.h"
#include "vulkan_texture.h"
#include "vulkan_image.h"
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

b8 vulkan_renderer_backend_initialize(box_renderer_backend* backend, box_renderer_backend_config* config) {
	backend->internal_context = ballocate(sizeof(vulkan_context), MEMORY_TAG_RENDERER);
	vulkan_context* context = (vulkan_context*)backend->internal_context;
	context->config = *config;

	if (backend->platform == NULL) {
		BX_ERROR("Offscreen renderering is not supported by the Vulkan backend");
		return FALSE;
	}

    // Global Vulkan init code (can technically span across renderer backends)
    // --------------------------------------
	const char** platform_extensions = NULL;
	u32 platform_extensions_count = vulkan_platform_get_required_extensions(&platform_extensions);

	// Obtain a list of required extensions
	const char** required_extensions = darray_from_data(const char*, platform_extensions_count, platform_extensions, MEMORY_TAG_RENDERER);
	const char** required_validation_layers = darray_create(const char*, MEMORY_TAG_RENDERER);

	// Add debug extensions/layers if enabled
	if (config->enable_validation) {
		darray_push(required_extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		darray_push(required_validation_layers, "VK_LAYER_KHRONOS_validation");
	}

	// Verify exsistence of validation layers
	u32 supported_layer_count = 0;
	vkEnumerateInstanceLayerProperties(&supported_layer_count, NULL);

	VkLayerProperties* supported_layers = darray_reserve(VkLayerProperties, supported_layer_count, MEMORY_TAG_RENDERER);
	vkEnumerateInstanceLayerProperties(&supported_layer_count, supported_layers);
	darray_length_set(supported_layers, supported_layer_count);

	for (u32 i = 0; i < darray_length(required_validation_layers); ++i) {
		b8 found = FALSE;
		for (u32 j = 0; j < darray_length(supported_layers); ++j) {
			if (strings_equal(required_validation_layers[i], supported_layers[j].layerName)) {
				found = TRUE;
				break;
			}
		}

		if (!found) {
			BX_ERROR("Required Vulkan validation layer is missing: %s.", required_validation_layers[i]);
			return FALSE;
		}
	}

	// Fill create info
	VkApplicationInfo app_info = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app_info.apiVersion = VK_API_VERSION_1_2;
	app_info.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
	app_info.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
	app_info.pApplicationName = config->application_name;
	app_info.pEngineName = "Boxel";

	VkInstanceCreateInfo create_info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	create_info.pApplicationInfo = &app_info;
	create_info.enabledExtensionCount = darray_length(required_extensions);
	create_info.ppEnabledExtensionNames = required_extensions;
	create_info.enabledLayerCount = darray_length(required_validation_layers);
	create_info.ppEnabledLayerNames = required_validation_layers;

	CHECK_VKRESULT(
		vkCreateInstance(
			&create_info, 
			context->allocator, 
			&context->instance), 
		"Failed to create Vulkan instance");

	// Clean up temp arrays
	darray_destroy(supported_layers);
	darray_destroy(required_extensions);
	darray_destroy(required_validation_layers);

	// Setup debug messenger if validation enabled
	if (config->enable_validation) {
		u32 log_severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;

		VkDebugUtilsMessengerCreateInfoEXT debug_create_info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
		debug_create_info.messageSeverity = log_severity;
		debug_create_info.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
		debug_create_info.pfnUserCallback = vk_debug_callback;

		PFN_vkCreateDebugUtilsMessengerEXT create_debug_messenger = 
			(PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context->instance, "vkCreateDebugUtilsMessengerEXT");

		CHECK_VKRESULT(
			create_debug_messenger(
				context->instance, 
				&debug_create_info, 
				context->allocator, 
				&context->debug_messenger),
			"Failed to create internal Vulkan debug messenger");
	}

	// Create the Vulkan device
	CHECK_VKRESULT(
		vulkan_device_create(backend),
		"Failed to create Vulkan device");
    // --------------------------------------

	
    // Vulkan window system / main rendertarget code.
    // --------------------------------------
	vulkan_rendertarget_attachment* main_attachments = darray_reserve(vulkan_rendertarget_attachment, config->main_attachment_count, MEMORY_TAG_RENDERER);

	if (backend->platform != NULL) {
		BX_ASSERT(backend->platform->internal_renderer_state == NULL && "Invalid state reached: Platform state already has renderer attached!");
	
		backend->platform->internal_renderer_state = ballocate(sizeof(vulkan_window_system), MEMORY_TAG_RENDERER);
		vulkan_window_system* window_system = (vulkan_window_system*)backend->platform->internal_renderer_state;

		CHECK_VKRESULT(
			vulkan_window_system_create(backend, window_system),
			"Failed to create internal Vulkan window surface / swapchain");
		
		vulkan_rendertarget_attachment swapchain_attachment = {
			.type = BOX_ATTACHMENT_COLOR,
			.images = window_system->images,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.format = window_system->swapchain_format.format,
			.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.store_op = VK_ATTACHMENT_STORE_OP_STORE,
			.stencil_load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencil_store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.final_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		};

		darray_push(main_attachments, swapchain_attachment);
	}

	for (u32 i = 0; i < config->main_attachment_count; ++i) {
		box_rendertarget_attachment* front_attachment = &config->main_attachments[i];

		vulkan_rendertarget_attachment attachment = {};
		attachment.type = front_attachment->type;
		attachment.format = box_render_format_to_vulkan_type(front_attachment->format);
		attachment.samples = VK_SAMPLE_COUNT_1_BIT; // TODO: Push to frontend renderer backend
		attachment.load_op = box_load_op_to_vulkan_type(front_attachment->load_op);
		attachment.store_op = box_store_op_to_vulkan_type(front_attachment->store_op);
		attachment.stencil_load_op = box_load_op_to_vulkan_type(front_attachment->stencil_load_op);
		attachment.stencil_store_op = box_store_op_to_vulkan_type(front_attachment->stencil_store_op);

		// TODO: ...
		attachment.final_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		VkImageUsageFlags image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		VkImageAspectFlags image_aspect = VK_IMAGE_ASPECT_COLOR_BIT;

		attachment.images = darray_reserve(vulkan_image, config->frames_in_flight, MEMORY_TAG_RENDERER);
		for (u32 i = 0; i < config->frames_in_flight; ++i) {
			vulkan_image* attachment_image = darray_push_empty(attachment.images);

			CHECK_VKRESULT(
				vulkan_image_create(
					context, 
					config->starting_size, 
					attachment.format, 
					image_usage, 
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
					TRUE, 
					image_aspect,
					attachment_image),
				"Failed to create main rendertarget attachments in Vulkan backend");
		}

		darray_push(main_attachments, attachment);
	}

	CHECK_VKRESULT(
		vulkan_rendertarget_create_internal(
			context, 
			TRUE, 
			(uvec2) { 0, 0 }, 
			config->starting_size, 
			darray_length(main_attachments),
			main_attachments, 
			&backend->main_rendertarget), 
		"Failed to create main rendertarget in Vulkan backend");
    // --------------------------------------

	context->in_flight_fences = darray_reserve(VkFence, config->frames_in_flight, MEMORY_TAG_RENDERER);

	for (u32 i = 0; i < config->frames_in_flight; ++i) {
		VkFence* fence = darray_push_empty(context->in_flight_fences);

		VkFenceCreateInfo fence_create_info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		CHECK_VKRESULT(
			vkCreateFence(
				context->device.logical_device,
				&fence_create_info,
				context->allocator,
				fence),
			"Failed to create Vulkan sync objects");
	}

	if (config->modes & RENDERER_MODE_GRAPHICS) {
		vulkan_queue_type queue_type = VULKAN_QUEUE_TYPE_GRAPHICS;
		context->graphics_command_ring = darray_reserve(vulkan_command_buffer, config->frames_in_flight, MEMORY_TAG_RENDERER);

		for (u32 i = 0; i < config->frames_in_flight; ++i) {
			// TODO: Allocate command buffer all at once.
			CHECK_VKRESULT(
				vulkan_command_buffer_allocate(
					context,
					&context->device.mode_queues[queue_type],
					TRUE,
					&context->graphics_command_ring[i]),
				"Failed to create Vulkan command buffers");

		}
	}

	if (config->modes & RENDERER_MODE_COMPUTE) {
		vulkan_queue_type queue_type = VULKAN_QUEUE_TYPE_COMPUTE;
		context->compute_command_ring = darray_reserve(vulkan_command_buffer, config->frames_in_flight, MEMORY_TAG_RENDERER);

		for (u32 i = 0; i < config->frames_in_flight; ++i) {
			// TODO: Allocate command buffer all at once.
			CHECK_VKRESULT(
				vulkan_command_buffer_allocate(
					context,
					&context->device.mode_queues[queue_type],
					TRUE,
					&context->compute_command_ring[i]),
				"Failed to create Vulkan command buffers");
		}
	}

    // Per frame structures (needs BIG improvements soon)
    // --------------------------------------
	context->memory_barriers = darray_create(memory_barrier, MEMORY_TAG_RENDERER);
	context->queued_submissions = darray_create(vulkan_queue_submission, MEMORY_TAG_RENDERER);
	context->semaphore_pool = darray_create(VkSemaphore, MEMORY_TAG_RENDERER);

	VkSemaphoreCreateInfo semaphore_create_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	for (u32 i = 0; i < VULKAN_QUEUE_TYPE_MAX * config->frames_in_flight; ++i) {
		VkSemaphore* semaphore = darray_push_empty(context->semaphore_pool);

		CHECK_VKRESULT(
			vkCreateSemaphore(
				context->device.logical_device,
				&semaphore_create_info,
				context->allocator,
				semaphore),
			"Failed to create Vulkan semaphore in pool");
	}
    // --------------------------------------
	return TRUE;
}

void vulkan_renderer_backend_shutdown(box_renderer_backend* backend) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;
	if (context->device.logical_device) vkDeviceWaitIdle(context->device.logical_device);

	// Destroy in the opposite order of creation.

    // Per frame structures
    // --------------------------------------
	if (context->semaphore_pool) {
		for (u32 i = 0; i < darray_length(context->semaphore_pool); ++i) {
			if (!context->semaphore_pool[i]) continue;

			vkDestroySemaphore(
				context->device.logical_device, 
				context->semaphore_pool[i], 
				context->allocator);
		}

		darray_destroy(context->semaphore_pool);
	}

	if (context->memory_barriers) darray_destroy(context->memory_barriers);

	if (context->queued_submissions) darray_destroy(context->queued_submissions);
    // --------------------------------------

	if (context->graphics_command_ring) {
		for (u32 i = 0; i < darray_length(context->graphics_command_ring); ++i) {
			vulkan_command_buffer_free(
				context, 
				&context->graphics_command_ring[i]);
		}

		darray_destroy(context->graphics_command_ring);
	}

	if (context->compute_command_ring) {
		for (u32 i = 0; i < darray_length(context->compute_command_ring); ++i) {
			vulkan_command_buffer_free(
				context, 
				&context->compute_command_ring[i]);
		}

		darray_destroy(context->compute_command_ring);
	}

	if (context->in_flight_fences) {
		for (u32 i = 0; i < darray_length(context->in_flight_fences); ++i) {
			if (!context->in_flight_fences[i]) continue;

			vkDestroyFence(
				context->device.logical_device,
				context->in_flight_fences[i],
				context->allocator);
		}

		darray_destroy(context->in_flight_fences);
	}

	vulkan_rendertarget_destroy(backend, &backend->main_rendertarget);

	if (backend->platform && backend->platform->internal_renderer_state) {
		vulkan_window_system* window_system = (vulkan_window_system*)backend->platform->internal_renderer_state;
		vulkan_window_system_destroy(backend, window_system);
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

	if (backend->platform != NULL) {
		vulkan_window_system* window_system = (vulkan_window_system*)backend->platform->internal_renderer_state;

		CHECK_VKRESULT(
			vulkan_window_system_acquire(
				backend, 
				window_system, 
				UINT64_MAX,
				VK_NULL_HANDLE),
			"Failed to accquire next Vulkan swapchain image");
	}
	
	darray_length_set(context->memory_barriers, 0);
	darray_length_set(context->queued_submissions, 0);
	context->last_mode = 0;
    return TRUE;
}

void vulkan_renderer_execute_command(box_renderer_backend* backend, box_rendercmd_context* rendercmd_context, rendercmd_header* header, rendercmd_payload* payload) {
    vulkan_context* context = (vulkan_context*)backend->internal_context;

	if (rendercmd_context->current_mode != context->last_mode) {
		// Create new queue submission
		vulkan_queue_submission* new = darray_push_empty(context->queued_submissions);

		switch (rendercmd_context->current_mode) {
			case RENDERER_MODE_GRAPHICS: 
				new->command_buffer = &context->graphics_command_ring[context->current_frame];
				break;

			case RENDERER_MODE_COMPUTE: 
				new->command_buffer = &context->compute_command_ring[context->current_frame];
				break;

			default:
				BX_ASSERT(FALSE && "Unsupported renderer mode");
				break;
		}

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

	if (backend->platform != NULL) {
		vulkan_window_system* window_system = (vulkan_window_system*)backend->platform->internal_renderer_state;
		darray_push(context->queued_submissions[0].wait_semaphores, window_system->image_available_semaphores[context->current_frame]);
	}

	for (u32 i = 0; i < darray_length(context->queued_submissions); ++i) {
		vulkan_queue_submission* submission = &context->queued_submissions[i];
		VkFence signal_fence = VK_NULL_HANDLE;

		CHECK_VKRESULT(
			vulkan_command_buffer_end(submission->command_buffer), 
			"Failed to end Vulkan command buffer");

		// TODO: Propbably change this at some point...
		VkPipelineStageFlags wait_flags[] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };

		if (i == darray_length(context->queued_submissions) - 1)
			signal_fence = context->in_flight_fences[context->current_frame];
		
		CHECK_VKRESULT(
			vulkan_command_buffer_submit(
				submission->command_buffer,
				darray_length(submission->wait_semaphores), 
				submission->wait_semaphores, 
				1, 
				&submission->signal_semaphore,
				wait_flags,
				signal_fence),
			"Failed to submit Vulkan command buffer");
		
		render_complete_semaphore = submission->signal_semaphore;
		darray_destroy(submission->wait_semaphores);
	}

	if (backend->platform != NULL) {
		vulkan_window_system* window_system = (vulkan_window_system*)backend->platform->internal_renderer_state;
	
		CHECK_VKRESULT(
			vulkan_window_system_present(
				backend, 
				window_system, 
				&context->device.mode_queues[VULKAN_QUEUE_TYPE_PRESENT], 
				1, 
				&render_complete_semaphore),
			"Failed to present Vulkan swapchain image");
	}	

	// Advance to next frame
    context->current_frame = (context->current_frame + 1) % context->config.frames_in_flight;
    return TRUE;
}