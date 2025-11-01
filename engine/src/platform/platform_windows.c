#include "defines.h"
#include "platform/platform.h"

#if BX_PLATFORM_WINDOWS

#include "engine.h"
#include "utils/darray.h"

#define GLFW_INCLUDE_VULKAN
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <string.h>

typedef struct internal_state {
	GLFWwindow* window;
} internal_state;

b8 platform_start(box_platform* plat_state, box_config* app_config) {
	plat_state->internal_state = platform_allocate(sizeof(internal_state), FALSE);
	internal_state* state = (internal_state*)plat_state->internal_state;

	int success = glfwInit();
	if (success == 0)
	{
		//printf("GLFW initialization failed");
		return FALSE;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	state->window = glfwCreateWindow(app_config->start_width, app_config->start_height, app_config->title, NULL, NULL);

	glfwMakeContextCurrent(state->window);
	return TRUE;
}

void platform_shutdown(box_platform* plat_state) {
	internal_state* state = (internal_state*)plat_state->internal_state;

	glfwDestroyWindow(state->window);

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

void platform_free(void* block, b8 aligned) {
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

void platform_console_write(const char* message, u8 colour) {
	HANDLE console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	// FATAL,ERROR,WARN,INFO,DEBUG,TRACE
	static u8 levels[6] = { 64, 4, 6, 2, 1, 8 };
	SetConsoleTextAttribute(console_handle, levels[colour]);
	OutputDebugStringA(message);
	u64 length = strlen(message);
	LPDWORD number_written = 0;
	WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), message, (DWORD)length, number_written, 0);
}

void platform_console_write_error(const char* message, u8 colour) {
	HANDLE console_handle = GetStdHandle(STD_ERROR_HANDLE);
	// FATAL,ERROR,WARN,INFO,DEBUG,TRACE
	static u8 levels[6] = { 64, 4, 6, 2, 1, 8 };
	SetConsoleTextAttribute(console_handle, levels[colour]);
	OutputDebugStringA(message);
	u64 length = strlen(message);
	LPDWORD number_written = 0;
	WriteConsoleA(GetStdHandle(STD_ERROR_HANDLE), message, (DWORD)length, number_written, 0);
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
