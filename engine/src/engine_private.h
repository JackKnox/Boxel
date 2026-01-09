#pragma once

#include "defines.h"

#include "engine.h"
#include "resource_system.h"

#include "platform/platform.h"
#include "renderer/renderer_cmd.h"

#include "core/allocators.h"

// Core engine context, contains all global state required for running the engine.
// Designed to be accessed primarily by the renderer and main/game threads.
typedef struct box_engine {
	// Engine state flags:
	// - is_running: True if the render thread is active and window is open.
	// - is_suspended: True if the engine is paused or window is minimized.
	// - should_quit: True when the engine is requested to stop execution.
	b8 is_running, is_suspended, should_quit;

	// Number of slots in the command ring buffer.
	u8 command_ring_length;

	// True if a frame is currently being processed.
	b8 has_open_frame;

	// Engine configuration (window size, vsync, etc.).
	box_config config;

	// Total memory allocated for engine subsystems.
	burst_allocator allocator;

	// Selected rendering backend (OpenGL, Vulkan, etc.).
	box_renderer_backend renderer;

	// Pointer to the engine's resource management system.
	box_resource_system* resource_system;

	// Platform-specific state (window handle, input devices, etc.).
	box_platform platform_state;

	// Render thread handle.
	box_thread render_thread;

	// Timing values in seconds.
	f64 last_time;    // Timestamp of previous frame
	f64 delta_time;   // Time elapsed since last frame

	// Ring buffer indices for multi-threaded command submission.
	u64 game_write_idx;   // Written by game thread
	u64 render_read_idx;  // Read by render thread

	// Current render command context (shader, mode, descriptors, etc.).
	rendercmd_context command_context;

	// Ring buffer for render commands. Fixed size; never resized at runtime.
	box_rendercmd* command_ring;

	// Synchronization primitives for command buffer access:
	// - rendercmd_mutex: Protects ring buffer access.
	// - rendercmd_cnd: Allows threads to wait for or signal command processing.
	box_mutex rendercmd_mutex;
	box_cond rendercmd_cnd;
} box_engine;

// Initializes the engine thread and internal systems.
b8 engine_thread_init(box_engine* engine);

// Shuts down the engine thread and cleans up all resources.
void engine_thread_shutdown(box_engine* engine);                                                          