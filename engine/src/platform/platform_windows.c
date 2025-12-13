#include "defines.h"
#include "platform/platform.h"

#if BX_PLATFORM_WINDOWS

#include "engine.h"
#include "utils/darray.h"

#include <windows.h>
#include <windowsx.h>  // param input extraction
#include <stdlib.h>
#include <stdio.h>

// For surface creation
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#include "renderer/vulkan/vulkan_types.h"

typedef struct internal_state {
	HINSTANCE h_instance;
	HWND hwnd;
} internal_state;

static f64 clock_frequency;
static LARGE_INTEGER start_time;

LRESULT CALLBACK win32_process_message(HWND hwnd, u32 msg, WPARAM w_param, LPARAM l_param);

void clock_setup() {
	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);
	clock_frequency = 1.0 / (f64)frequency.QuadPart;
	QueryPerformanceCounter(&start_time);
}

b8 platform_start(box_platform* plat_state, box_config* app_config) {
	plat_state->internal_state = platform_allocate(sizeof(internal_state), FALSE);
	platform_zero_memory(plat_state->internal_state, sizeof(internal_state));

	internal_state* state = (internal_state*)plat_state->internal_state;

	state->h_instance = GetModuleHandle(0);
	HICON icon = LoadIcon(state->h_instance, IDI_APPLICATION);

	WNDCLASS wc;
	platform_zero_memory(&wc, sizeof(WNDCLASS));
	wc.style = CS_DBLCLKS;  // Get double-clicks
	wc.lpfnWndProc = win32_process_message;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = state->h_instance;
	wc.hIcon = icon;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);  // NULL; // Manage the cursor manually
	wc.hbrBackground = NULL;                   // Transparent
	wc.lpszClassName = "boxel_window_class";

	if (!RegisterClassA(&wc)) {
		BX_ERROR("Window registration failed");
		return FALSE;
	}

	// Create window
	u32 client_x = app_config->window_position.absolute.x;
	u32 client_y = app_config->window_position.absolute.y;
	u32 client_width = app_config->window_size.x;
	u32 client_height = app_config->window_size.y;

	u32 window_x = client_x;
	u32 window_y = client_y;
	u32 window_width = client_width;
	u32 window_height = client_height;

	u32 window_style = WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION;
	u32 window_ex_style = WS_EX_APPWINDOW;

	window_style |= WS_MAXIMIZEBOX;
	window_style |= WS_MINIMIZEBOX;
	window_style |= WS_THICKFRAME;
	
	// Obtain the size of the border.
	RECT border_rect = { 0, 0, 0, 0 };
	AdjustWindowRectEx(&border_rect, window_style, 0, window_ex_style);

	// In this case, the border rectangle is negative.
	window_x += border_rect.left;
	window_y += border_rect.top;

	// Grow by the size of the OS border.
	window_width += border_rect.right - border_rect.left;
	window_height += border_rect.bottom - border_rect.top;

	state->hwnd = CreateWindowExA(
		window_ex_style, wc.lpszClassName, app_config->title,
		window_style, window_x, window_y, window_width, window_height,
		0, 0, state->h_instance, 0);

	if (state->hwnd == 0) {
		BX_ERROR("Window creation failed");
		return FALSE;
	}

	// Show the window
	b32 should_activate = 1;  // TODO: if the window should not accept input, this should be false.
	i32 show_window_command_flags = should_activate ? SW_SHOW : SW_SHOWNOACTIVATE;
	// If initially minimized, use SW_MINIMIZE : SW_SHOWMINNOACTIVE;
	// If initially maximized, use SW_SHOWMAXIMIZED : SW_MAXIMIZE
	ShowWindow(state->hwnd, show_window_command_flags);

	// Clock setup
	clock_setup();

	return TRUE;
}

void platform_show_window(box_platform* plat_state, b8 show) {
	internal_state* state = (internal_state*)plat_state->internal_state;
}

void platform_shutdown(box_platform* plat_state) {
	internal_state* state = (internal_state*)plat_state->internal_state;
	if (state && state->hwnd) {
		DestroyWindow(state->hwnd);
		state->hwnd = 0;
	}

	platform_free(plat_state->internal_state, FALSE);
}

b8 platform_pump_messages(box_platform* plat_state) {
	MSG message;
	while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&message);
		DispatchMessageA(&message);
	}

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
	if (!clock_frequency) {
		clock_setup();
	}

	LARGE_INTEGER now_time;
	QueryPerformanceCounter(&now_time);
	return (f64)now_time.QuadPart * clock_frequency;
}

void platform_sleep(u64 ms) {
	Sleep(ms);
}

void platform_get_vulkan_extensions(const char*** names_darray) {
	darray_push(*names_darray, &"VK_KHR_win32_surface");
}

VkResult platform_create_vulkan_surface(vulkan_context* context, box_platform* plat_state) {
	internal_state* state = (internal_state*)plat_state->internal_state;

	VkWin32SurfaceCreateInfoKHR create_info = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
	create_info.hinstance = state->h_instance;
	create_info.hwnd = state->hwnd;

	VkResult result = vkCreateWin32SurfaceKHR(context->instance, &create_info, context->allocator, &context->surface);
	if (!vulkan_result_is_success(result)) {
		BX_ERROR("Vulkan surface creation failed.");
		return FALSE;
	}

	return TRUE;
}

LRESULT CALLBACK win32_process_message(HWND hwnd, u32 msg, WPARAM w_param, LPARAM l_param) {
	switch (msg) {
	case WM_ERASEBKGND:
		// Notify the OS that erasing will be handled by the application to prevent flicker.
		return 1;
	case WM_CLOSE:
		event_context data = { 0 };
		event_fire(EVENT_CODE_APPLICATION_QUIT, 0, data);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_SIZE: {
		// Get the updated size.
		RECT r;
		GetClientRect(hwnd, &r);
		u32 width = r.right - r.left;
		u32 height = r.bottom - r.top;

		// Fire the event. The application layer should pick this up, but not handle it
		// as it shouldn be visible to other parts of the application.
		event_context context;
		context.data.u16[0] = (u16)width;
		context.data.u16[1] = (u16)height;
		event_fire(EVENT_CODE_RESIZED, 0, context);
	} break;
	}

	return DefWindowProcA(hwnd, msg, w_param, l_param);
}

#endif
