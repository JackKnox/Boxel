#pragma once 

#include "defines.h"

// Opaque structure holding platform-specific state (windowing, input, memory, timing, etc.).
typedef struct box_platform {
	// Internal implementation details (do not access directly).
	void* internal_state;
} box_platform;

// Window modes supported by the platform.
typedef enum box_window_mode {
    // Normal windowed mode.
    BOX_WINDOW_MODE_WINDOWED,

    // Window starts maximized.
    BOX_WINDOW_MODE_MAXIMIZED,

    // Fullscreen mode, covering the display.
    BOX_WINDOW_MODE_FULLSCREEN
} box_window_mode;

// Initializes the platform and starts the window based on the provided configuration.
b8 platform_start(box_platform* state, struct box_config* app_config);

// Shuts down the platform, closes windows, and frees all internal state.
void platform_shutdown(box_platform* plat_state);

// Processes platform messages (input, window events, etc.) and dispatches them to the engine.
b8 platform_pump_messages(box_platform* plat_state);

// Platform-level memory allocation.
void* platform_allocate(u64 size, b8 aligned);

// Frees memory allocated by platform_allocate.
void platform_free(const void* block, b8 aligned);

// Copies memory from source to destination.
void* platform_copy_memory(void* dest, const void* source, u64 size);

// Sets a block of memory to a specific value.
void* platform_set_memory(void* dest, i32 value, u64 size);

// Writes a message to the platform console/log.
void platform_console_write(log_level level, const char* message);

// Returns high-resolution absolute time in seconds since the start of the program.
f64 platform_get_absolute_time();

// Sleeps the calling thread for the specified number of milliseconds.
void platform_sleep(u64 ms);