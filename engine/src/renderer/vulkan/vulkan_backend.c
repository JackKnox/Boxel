#include "defines.h"
#include "vulkan_backend.h"

#include "engine.h"
#include "platform/platform.h"

#include "vulkan_types.inl"

b8 vulkan_renderer_backend_initialize(renderer_backend* backend, box_config* config) {
	backend->internal_context = platform_allocate(sizeof(vulkan_context), FALSE);
	vulkan_context* context = (vulkan_context*)backend->internal_context;

	VkApplicationInfo app_info = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app_info.apiVersion = VK_API_VERSION_1_2;
	app_info.pApplicationName = config->title;
	app_info.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
	app_info.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
	app_info.pEngineName = "Boxel";



	return TRUE;
}

void vulkan_renderer_backend_shutdown(renderer_backend* backend) {
}

void vulkan_renderer_backend_on_resized(renderer_backend* backend, u16 width, u16 height) {
}

b8 vulkan_renderer_backend_begin_frame(renderer_backend* backend, f32 delta_time) {
	return TRUE;
}

b8 vulkan_renderer_backend_end_frame(renderer_backend* backend) {
	return TRUE;
}
