#include "defines.h"
#include "engine_private.h"

#include "utils/darray.h"

// Sleep helper
void sleep_for_ms(long ms) {
	if (ms <= 0) return;
	struct timespec ts;
	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (ms % 1000) * 1000000L;
	thrd_sleep(&ts, NULL);
}

b8 engine_thread_init(box_engine* e) {
	renderer_backend* backend = &e->render_state;

	if (!platform_start(&e->platform_state, &e->config)) {
		BX_ERROR("Something went wrong with platform, exiting...");
		return FALSE;
	}
	
	if (!renderer_backend_create(e->config.render_config.api_type, &e->platform_state, &e->render_state)) {
		BX_ERROR("Could not allocate with render backend, exiting...");
		return FALSE;
	}

	e->config.render_config.framebuffer_width = e->config.window_size.x;
	e->config.render_config.framebuffer_height = e->config.window_size.y;

	if (!e->render_state.initialize(&e->render_state, e->config.title, &e->config.render_config)) {
		BX_ERROR("Something went wrong with render backend, exiting...");
		return FALSE;
	}

	const double target_frame_time = (e->config.target_fps > 0)
		? (1000.0 / (double)e->config.target_fps)
		: 0.0; // ms per frame

	// Send message to main thread to unlock user access to engine
	// since everthing is initialized
	e->is_running = TRUE;
	mtx_lock(&e->rendercmd_mutex);
	cnd_broadcast(&e->rendercmd_cv);
	mtx_unlock(&e->rendercmd_mutex);

	// Now that main thread is unlocked, wait until first box_rendercmd
	// to avoid UB when sending to renderer backend (Vulkan doesn't accept empty command buffers).
	mtx_lock(&e->rendercmd_mutex);
	while (!e->first_cmd_sent && !e->should_quit) {
		cnd_wait(&e->rendercmd_cv, &e->rendercmd_mutex);
	}
	mtx_unlock(&e->rendercmd_mutex);

	if (e->should_quit) return FALSE;

	e->last_time = platform_get_absolute_time();

	while (e->is_running && !e->should_quit) {
		f64 frame_start = platform_get_absolute_time();
		f64 delta_ms = frame_start - e->last_time;
		e->last_time = frame_start;
		e->delta_time = delta_ms;

		if (backend->begin_frame(backend, e->delta_time)) {
			// Get next read index reliably
			mtx_lock(&e->rendercmd_mutex);

			e->render_read_idx = (e->render_read_idx + 1) % FRAME_COUNT;

			// Wait until the writer has written to that slot (i.e., not equal to write index).
			while (e->render_read_idx == e->game_write_idx && e->is_running && !e->should_quit) {
				cnd_wait(&e->rendercmd_cv, &e->rendercmd_mutex);
			}

			// Grab a snapshot of the command->finished under the lock and then unlock
			box_rendercmd* command = &e->command_queue[e->render_read_idx];
			b8 has_finished = command->finished;
			mtx_unlock(&e->rendercmd_mutex);

			if (has_finished) {
				if (!backend->playback_rendercmd(backend, command)) {
					BX_ERROR("Could not playback render command, exiting...");
					e->should_quit = TRUE;
					return FALSE;
				}
			}

			// After successful playback:
			mtx_lock(&e->rendercmd_mutex);
			command->finished = FALSE; // Mark slot free for reuse
			// Signal any waiting writer that a slot became available
			cnd_signal(&e->rendercmd_cv);
			mtx_unlock(&e->rendercmd_mutex);

			b8 result = backend->end_frame(backend);
			if (!result) {
				BX_ERROR("Could not finish render frame, exiting...");
				e->should_quit = TRUE;
				return FALSE;
			}
		}

		platform_pump_messages(&e->platform_state);

		// Throttle FPS if configured (no sleep if 0 = uncapped)
		if (target_frame_time > 0.0) {
			f64 frame_end = platform_get_absolute_time();
			f64 elapsed = frame_end - frame_start;
			f64 remaining = target_frame_time - elapsed;
			if (remaining > 0.0)
				sleep_for_ms((long)remaining);
		}
	}

	return TRUE;
}

void engine_thread_shutdown(box_engine* e) {
	renderer_backend* backend = &e->render_state;
	backend->shutdown(&e->render_state);

	renderer_backend_destroy(&e->render_state);

	platform_shutdown(&e->platform_state);
}
