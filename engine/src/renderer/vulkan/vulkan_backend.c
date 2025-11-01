#include "defines.h"
#include "vulkan_backend.h"

#include "engine.h"
#include "platform/platform.h"

#include "vulkan_types.inl"

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

b8 vulkan_renderer_backend_initialize(renderer_backend* backend, box_engine* engine) {
	backend->internal_context = platform_allocate(sizeof(vulkan_context), FALSE);
	platform_zero_memory(backend->internal_context, sizeof(vulkan_context));

	box_config* config = box_engine_get_config(engine);
	vulkan_context* context = (vulkan_context*)backend->internal_context;
	context->allocator = 0;
	
	VkApplicationInfo app_info = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app_info.apiVersion = VK_API_VERSION_1_2;
	app_info.pApplicationName = config->title;
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
	if (config->enable_validation) {
		darray_push(required_extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		darray_push(required_validation_layer_names, "VK_LAYER_KHRONOS_validation");
	}

	// Fill create info
	create_info.enabledExtensionCount = darray_length(required_extensions);
	create_info.ppEnabledExtensionNames = required_extensions;
	create_info.enabledLayerCount = darray_length(required_validation_layer_names);
	create_info.ppEnabledLayerNames = required_validation_layer_names;

	VK_CHECK(vkCreateInstance(&create_info, context->allocator, &context->instance));

	// Setup debug messenger if validation enabled
	if (config->enable_validation) {
		u32 log_severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;

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
	}

	// Create the Vulkan surface
	platform_create_vulkan_surface(context->instance, backend->plat_state, context->allocator, &context->surface);

	// Clean up temp arrays
	darray_destroy(required_extensions);
	darray_destroy(required_validation_layer_names);

	return TRUE;
}

void vulkan_renderer_backend_shutdown(renderer_backend* backend) {
	vulkan_context* context = (vulkan_context*)backend->internal_context;

	vkDestroyInstance(context->instance, context->allocator);
}

void vulkan_renderer_backend_on_resized(renderer_backend* backend, u16 width, u16 height) {
}

b8 vulkan_renderer_backend_begin_frame(renderer_backend* backend, f32 delta_time) {
	return TRUE;
}

b8 vulkan_renderer_backend_end_frame(renderer_backend* backend) {
	return TRUE;
}
