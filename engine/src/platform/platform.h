#pragma once 

#include "defines.h"

// TODO: Make your own threading library.
#include <tinycthread.h>

typedef struct box_platform {
	void* internal_state;
} box_platform;

struct box_config;

// Starts window and it's specific platform.
b8 platform_start(box_platform* state, struct box_config* app_config);

// Shutdown and cleans platform-specific state
void platform_shutdown(box_platform* plat_state);

// Send messages from platform through out the engine
b8 platform_pump_messages(box_platform* plat_state);

void* platform_allocate(u64 size, b8 aligned);
void platform_free(void* block, b8 aligned);
void* platform_zero_memory(void* block, u64 size);
void* platform_copy_memory(void* dest, const void* source, u64 size);
void* platform_set_memory(void* dest, i32 value, u64 size);

void platform_console_write(const char* message, u8 colour);
void platform_console_write_error(const char* message, u8 colour);

f64 platform_get_absolute_time();
void platform_sleep(u64 ms);

void platform_get_vulkan_extensions(const char*** names_darray);