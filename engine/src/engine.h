#pragma once

#include "defines.h"

#include "renderer/renderer_backend.h"

// Application configuration
typedef struct box_config {
	// Window starting position x axis.
	u32 start_pos_x;

	// Window starting position y axis.
	u32 start_pos_y;

	// Window starting width.
	u32 start_width;

	// Window starting height.
	u32 start_height;

	// The application name used in windowing.
	const char* title;

	// FPS to lock render thread too.
	u32 target_fps;

	// Configuration for render backend.
	renderer_backend_config render_config;
} box_config;

// Opaque handle to true box_engine.
typedef struct box_engine box_engine;

// Sets default configurtion for Boxel.
box_config box_default_config();

// Creates and initializes the boxel engine.
box_engine* box_create_engine(box_config* app_config);

// Checks if specified engine is currently running.
b8 box_engine_is_running(box_engine* engine);

// Gets current config for specified engine.
const box_config* box_engine_get_config(box_engine* engine);

// Destroys the boxel engine and it's subsystem.
void box_destroy_engine(box_engine* engine);

