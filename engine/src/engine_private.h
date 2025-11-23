#pragma once

#include "defines.h"

#include "engine.h"

#include "platform/platform.h"
#include "renderer/renderer_cmd.h"

typedef struct box_engine {
	// Running = render thread actually open or not.
	// Suspended = window minimising / paused and waiting to be resumed.
	// Should quit = Thread is in the process of stopping execution.
	b8 is_running, is_suspended;
	b8 should_quit;

	box_config config;
	renderer_backend renderer;
	box_platform platform_state;

	thrd_t render_thread;
	f64 last_time, delta_time;

	u64 game_write_idx, render_read_idx;
	b8 first_cmd_sent;
	box_rendercmd* command_ring; // Constant size, cannot change size
	u8 command_ring_length; 

	mtx_t rendercmd_mutex; // For protecting rendercmd processing in a ring buffer.
	cnd_t rendercmd_cnd;   // Condition variable for above mutex to wait on for special processes.
} box_engine;

b8 engine_thread_init(box_engine* engine);

void engine_thread_shutdown(box_engine* engine);

#define WAIT_ON_CND(condition)                                          \
	{                                                                   \
		mtx_lock(&engine->rendercmd_mutex);                             \
		while (condition)                                               \
			cnd_wait(&engine->rendercmd_cnd, &engine->rendercmd_mutex); \
		mtx_unlock(&engine->rendercmd_mutex);                           \
	}                                                                  