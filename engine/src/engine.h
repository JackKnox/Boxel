#pragma once

#include "defines.h"

#include "renderer/renderer_backend.h"

// Application configuration
typedef struct box_config {
	// Window starting position.
	union box_windowpos {
		uvec2 absolute;
		b8 centered;
	} window_position;

	// Window starting mode.
	box_window_mode window_mode;

	// Window starting size.
	uvec2 window_size;

	// The application name used in windowing.
	const char* title;

	// FPS to lock render thread to.
	u32 target_fps;

	// Configuration for render backend.
	renderer_backend_config render_config;
} box_config;

// Opaque handle to true box_engine.
typedef struct box_engine box_engine;

// Sets default configurtion for Boxel.
box_config box_default_config();

// Creates and initializes the boxel engine, Consumes specified box_config.
box_engine* box_create_engine(box_config* app_config);

// Checks if specified engine is currently running.
b8 box_engine_is_running(box_engine* engine);

// Gets current config for specified engine.
const box_config* box_engine_get_config(box_engine* engine);

// Returns a per-frame render command buffer for the user to fill with rendering instructions.
struct box_rendercmd* box_engine_next_rendercmd(box_engine* engine);

// Renders next frame based on specified command.
void box_engine_render_frame(box_engine* engine, struct box_rendercmd* command);

// Destroys the boxel engine and it's subsystem.
void box_destroy_engine(box_engine* engine);

