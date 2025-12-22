#pragma once

#include "defines.h"

#include "engine.h"

#include "platform/platform.h"
#include "renderer/renderer_cmd.h"
#include "resource_system.h"

typedef struct box_engine {
	// Running = render thread actually open or not.
	// Suspended = window minimising / paused and waiting to be resumed.
	// Should quit = Thread is in the process of stopping execution.
	b8 is_running, is_suspended;
	b8 should_quit;
	u8 command_ring_length;

	box_config config;
	u64 allocation_size;

	box_renderer_backend renderer;
	box_resource_system resource_system;
	box_platform platform_state;

	thrd_t render_thread;
	f64 last_time, delta_time;

	u64 game_write_idx, render_read_idx;
	box_rendercmd* command_ring; // Constant size, cannot change size

	mtx_t rendercmd_mutex; // For protecting rendercmd processing in a ring buffer.
	cnd_t rendercmd_cnd;   // Condition variable for above mutex to wait on for special processes.
} box_engine;

b8 engine_thread_init(box_engine* engine);

void engine_thread_shutdown(box_engine* engine);                                                          