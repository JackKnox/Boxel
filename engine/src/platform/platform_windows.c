#include "defines.h"
#include "platform/platform.h"

#if BX_PLATFORM_WINDOWS

#include "engine.h"
#include "utils/darray.h"

#define GLFW_INCLUDE_VULKAN
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct internal_state {
	GLFWwindow* window;
} internal_state;

void GLFWErrorCallback(int error, const char* description) {
	BX_ERROR("%s", description);
}

b8 on_window_close(GLFWwindow* window) {
	event_context data = {0};
	event_fire(EVENT_CODE_APPLICATION_QUIT, 0, data);

	return FALSE;
}

b8 platform_start(box_platform* plat_state, box_config* app_config) {
	plat_state->internal_state = platform_allocate(sizeof(internal_state), FALSE);
	platform_zero_memory(plat_state->internal_state, sizeof(internal_state));
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
	if (!app_config->window_position.centered) {
		glfwWindowHint(GLFW_POSITION_X, app_config->window_position.absolute.x);
		glfwWindowHint(GLFW_POSITION_Y, app_config->window_position.absolute.y);
	} else {
		
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);

		int monitorX, monitorY;
		glfwGetMonitorPos(monitor, &monitorX, &monitorY);
		
		glfwWindowHint(GLFW_POSITION_X, monitorX + (mode->width - app_config->window_size.x) / 2);
		glfwWindowHint(GLFW_POSITION_Y, monitorY + (mode->height - app_config->window_size.y) / 2);
	}

	state->window = glfwCreateWindow(app_config->window_size.x, app_config->window_size.y, app_config->title, app_config->window_mode == BOX_WINDOW_MODE_FULLSCREEN  ? monitor : NULL, NULL);
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
	platform_free(plat_state->internal_state, FALSE);
}

b8 platform_pump_messages(box_platform* plat_state) {
	glfwPollEvents();
	return TRUE;
}

void* platform_allocate(u64 size, b8 aligned) {
	return malloc(size);
}

void platform_free(const void* block, b8 aligned) {
	free(block);
}

void* platform_zero_memory(void* block, u64 size) {
	return memset(block, 0, size);
}

void* platform_copy_memory(void* dest, const void* source, u64 size) {
	return memcpy(dest, source, size);
}

void* platform_set_memory(void* dest, i32 value, u64 size) {
	return memset(dest, value, size);
}

void platform_console_write(log_level level, const char* message) {
	b8 is_error = (level <= LOG_LEVEL_ERROR);
	HANDLE console = GetStdHandle(is_error ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);

	static const char* colors[] = {
		"\033[1;37;101m", // fatal
		"\033[1;31m",     // error
		"\033[1;33m",     // warn
		"\033[1;32m",     // info
		"\033[1;36m"      // trace
	};

	printf("%s%s\033[0m", colors[level], message);
}

f64 platform_get_absolute_time() {
	return 1000.0f * glfwGetTime();
}

void platform_sleep(u64 ms) {
	Sleep(ms);
}

void platform_get_vulkan_extensions(const char*** names_darray) {
	darray_push(*names_darray, &"VK_KHR_win32_surface");
}

b8 platform_create_vulkan_surface(VkInstance instance, box_platform* plat_state, const VkAllocationCallbacks* allocator, VkSurfaceKHR* out_surface) {
	internal_state* state = (internal_state*)plat_state->internal_state;

	VkResult result = glfwCreateWindowSurface(instance, state->window, allocator, out_surface);
	return result == VK_SUCCESS;
}

#endif
