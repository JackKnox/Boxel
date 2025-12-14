#pragma once 

#include "defines.h"

// TODO: Make your own threading library.
#include "tinycthread.h"

typedef struct box_platform {
	void* internal_state;
} box_platform;

typedef enum box_window_mode {
	BOX_WINDOW_MODE_WINDOWED,
	BOX_WINDOW_MODE_MAXIMIZED,
	BOX_WINDOW_MODE_FULLSCREEN
} box_window_mode;

// TODO: Seperate window from platform state.

// Starts window and it's specific platform.
b8 platform_start(box_platform* state, struct box_config* app_config);

void platform_show_window(box_platform* state, b8 show);

// Shutdown and cleans platform-specific state
void platform_shutdown(box_platform* plat_state);

// Send messages from platform through out the engine
b8 platform_pump_messages(box_platform* plat_state);

void* platform_allocate(u64 size, b8 aligned);
void platform_free(const void* block, b8 aligned);
void* platform_zero_memory(void* block, u64 size);
void* platform_copy_memory(void* dest, const void* source, u64 size);
void* platform_set_memory(void* dest, i32 value, u64 size);

void platform_console_write(log_level level, const char* message);

f64 platform_get_absolute_time();
void platform_sleep(u64 ms);

void platform_get_vulkan_extensions(const char*** names_darray);