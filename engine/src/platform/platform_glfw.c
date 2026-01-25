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

void on_window_close(GLFWwindow* window) {
	event_context data = { 0 };
	event_fire(EVENT_CODE_APPLICATION_QUIT, 0, data);
}

void on_key(GLFWwindow* window, int key, int scancode, int action, int mods) {
	event_context context = { 0 };
	context.data.u16[0] = (u16)key;

	switch (action) {
	case GLFW_PRESS:
		event_fire(EVENT_CODE_KEY_PRESSED, 0, context);
		break;
	case GLFW_RELEASE:
		event_fire(EVENT_CODE_KEY_RELEASED, 0, context);
		break;
	case GLFW_REPEAT:
		event_fire(EVENT_CODE_KEY_PRESSED, 0, context);
		break;
	}
}

void on_mouse_button(GLFWwindow* window, int button, int action, int mods) {
	event_context context = { 0 };
	context.data.u16[0] = (u16)button;

	if (action == GLFW_PRESS) {
		event_fire(EVENT_CODE_BUTTON_PRESSED, 0, context);
	}
	else if (action == GLFW_RELEASE) {
		event_fire(EVENT_CODE_BUTTON_RELEASED, 0, context);
	}
}

void on_cursor_position(GLFWwindow* window, double xpos, double ypos) {
	event_context context = { 0 };
	context.data.u16[0] = (u16)xpos;
	context.data.u16[1] = (u16)ypos;

	event_fire(EVENT_CODE_MOUSE_MOVED, 0, context);
}

void on_scroll(GLFWwindow* window, double xoffset, double yoffset) {
	event_context context = { 0 };
	context.data.u8[0] = (i8)yoffset;

	event_fire(EVENT_CODE_MOUSE_WHEEL, 0, context);
}

void on_window_resize(GLFWwindow* window, int width, int height) {
	event_context context = { 0 };
	context.data.u16[0] = (u16)width;
	context.data.u16[1] = (u16)height;

	event_fire(EVENT_CODE_RESIZED, 0, context);
}

b8 platform_start(box_platform* plat_state, box_config* app_config) {
	BX_ASSERT(plat_state != NULL && app_config != NULL && "Invalid arguments passed to platform_start");
	plat_state->internal_state = ballocate(sizeof(internal_state), MEMORY_TAG_PLATFORM);
	internal_state* state = (internal_state*)plat_state->internal_state;

	glfwSetErrorCallback(GLFWErrorCallback);
	if (glfwInit() == 0) {
		BX_ERROR("Failed initializing GLFW.");
		return FALSE;
	}

	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);

	int monitorX, monitorY;
	glfwGetMonitorPos(monitor, &monitorX, &monitorY);

	uvec2 window_pos = app_config->window_absolute;
	if (app_config->window_centered) {
		window_pos.x = monitorX + (mode->width - app_config->window_size.x) / 2;
		window_pos.y = monitorY + (mode->height - app_config->window_size.y) / 2;
	}

	if (window_pos.x > monitorX + mode->width || window_pos.y > monitorY + mode->height) {
		BX_ERROR("Specififed window position is outside range of monitior");
		return FALSE;
	}

	if (app_config->window_size.width == mode->width || app_config->window_size.height == mode->height) {
		BX_ERROR("Specififed window size is larger than size of monitior");
		return FALSE;
	}

	glfwWindowHint(GLFW_CLIENT_API, app_config->render_config.api_type == RENDERER_BACKEND_TYPE_OPENGL ? GLFW_OPENGL_API : GLFW_NO_API);
	glfwWindowHint(GLFW_MAXIMIZED, app_config->window_mode == BOX_WINDOW_MODE_MAXIMIZED);
	glfwWindowHint(GLFW_POSITION_X, window_pos.x);
	glfwWindowHint(GLFW_POSITION_Y, window_pos.y);

	state->window = glfwCreateWindow(app_config->window_size.width, app_config->window_size.height, app_config->title, app_config->window_mode == BOX_WINDOW_MODE_FULLSCREEN ? monitor : NULL, NULL);
	if (!state->window) {
		BX_ERROR("Failed creating application window.");
		return FALSE;
	}

	glfwSetWindowCloseCallback(state->window, on_window_close);
	glfwSetKeyCallback(state->window, on_key);
	glfwSetMouseButtonCallback(state->window, on_mouse_button);
	glfwSetCursorPosCallback(state->window, on_cursor_position);
	glfwSetScrollCallback(state->window, on_scroll);
	glfwSetWindowSizeCallback(state->window, on_window_resize);

	return TRUE;
}

void platform_shutdown(box_platform* plat_state) {
	BX_ASSERT(plat_state != NULL && "Invalid arguments passed to platform_shutdown");
	internal_state* state = (internal_state*)plat_state->internal_state;
	if (state->window) glfwDestroyWindow(state->window);

	glfwTerminate();
	bfree(plat_state->internal_state, sizeof(internal_state), MEMORY_TAG_PLATFORM);
}

b8 platform_pump_messages(box_platform* plat_state) {
	BX_ASSERT(plat_state != NULL && "Invalid arguments passed to platform_pump_messages");
	glfwPollEvents();
	return TRUE;
}

f64 platform_get_absolute_time() {
	return 1000.0f * glfwGetTime();
}

VkResult platform_create_vulkan_surface(vulkan_context* context, box_platform* plat_state) {
	BX_ASSERT(context != NULL && plat_state != NULL && "Invalid arguments passed to platform_create_vulkan_surface");
	internal_state* state = (internal_state*)plat_state->internal_state;
	return glfwCreateWindowSurface(context->instance, state->window, context->allocator, &context->surface);
}

void platform_get_vulkan_extensions(const char*** names_darray) {
	BX_ASSERT(names_darray != NULL && "Invalid arguments passed to platform_get_vulkan_extensions");
	uint32_t count;
	const char** extensions = glfwGetRequiredInstanceExtensions(&count);

	for (u32 i = 0; i < count; ++i) {
		darray_push(*names_darray, extensions[i]);
	}
}
