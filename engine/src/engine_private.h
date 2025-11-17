#pragma once

#include "defines.h"

#include "engine.h"

#include "platform/platform.h"
#include "renderer/renderer_cmd.h"

typedef struct box_engine {
	b8 is_running;
	b8 should_quit; // TODO: atomic_flag quit_signal
	box_config config; // Will change to affect the current state
	box_platform platform_state;

	renderer_backend render_state;
	thrd_t render_thread;
	mtx_t rendercmd_mutex;
	cnd_t rendercmd_cv;

	box_rendercmd* command_queue; // circler queue
	u32 game_write_idx;
	u32 render_read_idx;
	b8 frame_ready;

	f64 last_time;
	f64 delta_time;
} box_engine;

b8 engine_thread_init(box_engine* engine);

void engine_thread_shutdown(box_engine* engine);

u8 engine_frame_count(box_engine* engine);