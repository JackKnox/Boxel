#pragma once

#include "defines.h"

#include "platform/platform.h"

#include "renderer/renderer_backend.h"
#include "renderer/renderer_cmd.h"

#include "resource_system.h"

// Application configuration used to initialize a Boxel engine instance.
typedef struct box_config {
	// Window starting position.
	// Either absolute coordinates or centered on screen.
	union box_windowpos {
		uvec2 absolute; // Position in pixels from top-left corner.
		b8 centered;    // True to center window automatically.
	} window_position;

	// Initial window mode (e.g., windowed, fullscreen, borderless).
	box_window_mode window_mode;

	// Target FPS to lock the render thread to.
	u32 target_fps;

	// Initial window size in pixels.
	uvec2 window_size;

	// Application title used for windowing and OS integration.
	const char* title;

	// Renderer-specific configuration (API-specific settings).
	box_renderer_backend_config render_config;

	// Print diagnostics on engine start.
	b8 output_diagnostics;
} box_config;

// Opaque handle to a Boxel engine instance.
typedef struct box_engine box_engine;

// Returns a default configuration for initializing Boxel.
box_config box_default_config();

// Creates and initializes a Boxel engine using the specified configuration, ownership of the returned engine is transferred to the caller.
box_engine* box_create_engine(box_config* app_config);

// Returns true if the engine's main render thread is currently running.
b8 box_engine_is_running(box_engine* engine);

// Returns true if the engine is temporarily paused or suspended (e.g., window minimized).
b8 box_engine_should_skip_frame(box_engine* engine);

// Closes the window associated with the engine if should_close is true; box_destroy_engine must still be called to fully clean up.
void box_close_engine(box_engine* engine, b8 should_close);

// Returns the engine's resource system for direct resource management.
box_resource_system* box_engine_get_resource_system(box_engine* engine);

// Blocks until the engine and all dependent subsystems are fully initialized, useful for asynchronous resource setup before the first frame
void box_engine_prepare_resources(box_engine* engine);

// Outputs diagnostic information about the current state of the engine.
void box_engine_output_diagnostics(box_engine* engine);

// Returns a pointer to the engine's current configuration.
const box_config* box_engine_get_config(box_engine* engine);

// Returns a per-frame render command buffer for the user to populate with rendering instructions for the current frame.
box_rendercmd* box_engine_next_frame(box_engine* engine);

// Sends render command back to engine returned by box_rendercmd for rendering.
void box_engine_end_frame(box_engine* engine);

// Destroys the engine and all its subsystems, freeing all memory. After this call, the engine pointer is invalid and must not be used.
void box_destroy_engine(box_engine* engine);

