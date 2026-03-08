#pragma once 

#include "defines.h"

// Opaque structure holding platform-specific state (windowing, input, memory, timing, etc.).
typedef struct box_platform {
	// Internal implementation details for window (do not access directly).
	void* internal_state;

	// Pointer to platform renderer context, not owned by platform (do not access directly).
	void* internal_renderer_state;
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

// Application configuration used to initialize a Boxel engine instance.
typedef struct box_window_config {
	// Window starting position.
	// Either absolute coordinates or centered on screen.
	union {
		// Position in pixels from top-left corner.
		uvec2 window_absolute;
			
		// True to center window automatically.
		b8 window_centered;
	};

	// Initial window mode (e.g., windowed, fullscreen, borderless).
	box_window_mode window_mode;

	// Initial window size in pixels.
	uvec2 window_size;

	// Application title used for windowing and OS integration.
	const char* title;
} box_window_config;

// Returns a default configuration for initializing Boxel.
box_window_config box_window_default_config();

// Initializes the platform and starts the window based on the provided configuration.
b8 platform_start(box_platform* state, box_window_config* app_config);

// Shuts down the platform, closes windows, and frees all internal state.
void platform_shutdown(box_platform* plat_state);

// Processes platform messages (input, window events, etc.) and dispatches them to the engine.
b8 platform_pump_messages(box_platform* plat_state);

// This function queries the platform-specific window state and returns the platform window should close.
b8 platform_should_close_window(box_platform* plat_state);

// Platform-level memory allocation.
void* platform_allocate(u64 size, b8 aligned);

// Frees memory allocated by platform_allocate.
void platform_free(const void* block, b8 aligned);

// Copies memory from source to destination.
void* platform_copy_memory(void* dest, const void* source, u64 size);

// Sets a block of memory to a specific value.
void* platform_set_memory(void* dest, i32 value, u64 size);

// Checks if two blocks are the same data exactlly.
b8 platform_compare_memory(void* buf1, void* buf2, u64 size);

// Writes a message to the platform console/log.
void platform_console_write(log_level level, const char* message);

// Returns high-resolution absolute time in seconds since the start of the program.
f64 platform_get_absolute_time();

// Sleeps the calling thread for the specified number of milliseconds.
void platform_sleep(u64 ms);