#include "defines.h"
#include "platform/platform.h"

#include "engine.h"
#include "utils/darray.h"

#define GLFW_INCLUDE_VULKAN
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>

#include "renderer/vulkan/vulkan_types.h"

#include <stdlib.h>
#include <stdio.h>

typedef struct internal_state {
	GLFWwindow* window;
} internal_state;

void GLFWErrorCallback(int error, const char* description) {
	BX_ERROR("%s", description);
}

b8 on_window_close(GLFWwindow* window) {
	event_context data = { 0 };
	event_fire(EVENT_CODE_APPLICATION_QUIT, 0, data);

	return FALSE;
}

b8 platform_start(box_platform* plat_state, box_config* app_config) {
	plat_state->internal_state = ballocate(sizeof(internal_state), MEMORY_TAG_PLATFORM);
	internal_state* state = (internal_state*)plat_state->internal_state;

	glfwSetErrorCallback(GLFWErrorCallback);
	if (glfwInit() == 0) {
		BX_ERROR("Failed initializing GLFW.");
		return FALSE;
	}

	if (app_config->title == NULL) {
		BX_ERROR("Cannot set box_config->title to 0");
		return FALSE;
	}
	if (app_config->window_size.x == 0 || app_config->window_size.y == 0) {
		BX_ERROR("Cannot set box_config->window_size axis to 0");
		return FALSE;
	}

	GLFWmonitor* monitor = glfwGetPrimaryMonitor();

	glfwWindowHint(GLFW_CLIENT_API, app_config->render_config.api_type == RENDERER_BACKEND_TYPE_OPENGL ? GLFW_OPENGL_API : GLFW_NO_API);
	glfwWindowHint(GLFW_MAXIMIZED, app_config->window_mode == BOX_WINDOW_MODE_MAXIMIZED);
	glfwWindowHint(GLFW_POSITION_X, app_config->window_position.absolute.x);
	glfwWindowHint(GLFW_POSITION_Y, app_config->window_position.absolute.y);

	if (app_config->window_position.centered) {
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);

		int monitorX, monitorY;
		glfwGetMonitorPos(monitor, &monitorX, &monitorY);

		glfwWindowHint(GLFW_POSITION_X, monitorX + (mode->width - app_config->window_size.x) / 2);
		glfwWindowHint(GLFW_POSITION_Y, monitorY + (mode->height - app_config->window_size.y) / 2);
	}

	state->window = glfwCreateWindow(app_config->window_size.x, app_config->window_size.y, app_config->title, app_config->window_mode == BOX_WINDOW_MODE_FULLSCREEN ? monitor : NULL, NULL);
	if (!state->window) {
		BX_ERROR("Failed creating application window.");
		return FALSE;
	}

	glfwSetWindowCloseCallback(state->window, on_window_close);

	return TRUE;
}

void platform_shutdown(box_platform* plat_state) {
	internal_state* state = (internal_state*)plat_state->internal_state;
	if (state->window) glfwDestroyWindow(state->window);

	glfwTerminate();
	bfree(plat_state->internal_state, sizeof(internal_state), MEMORY_TAG_PLATFORM);
}

b8 platform_pump_messages(box_platform* plat_state) {
	glfwPollEvents();
	return TRUE;
}

f64 platform_get_absolute_time() {
	return 1000.0f * glfwGetTime();
}

VkResult platform_create_vulkan_surface(vulkan_context* context, box_platform* plat_state) {
	internal_state* state = (internal_state*)plat_state->internal_state;
	return glfwCreateWindowSurface(context->instance, state->window, context->allocator, &context->surface);
}