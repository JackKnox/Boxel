#include "defines.h"
#include "engine_private.h"

#include "utils/darray.h"

// NOTE: Engine thread should never call functions like box_destroy_engine
//       which deallocate memory that main thread uses. When finding a fatal error
//       use "engine->should_quit = TRUE" and return to notify main thread to destroy
//       at a time which is safe for the program (see tag "exit_and_cleanup").

b8 get_next_rendercmd(box_engine* engine, box_rendercmd** out_rendercmd) {	
	// Get next read index reliably
	mtx_lock(&engine->rendercmd_mutex);

	engine->render_read_idx = (engine->render_read_idx + 1) % engine->command_ring_length;

	// Wait until the writer has written to that slot (i.e., not equal to write index).
	while (engine->render_read_idx == engine->game_write_idx && engine->is_running && !engine->should_quit)
		cnd_wait(&engine->rendercmd_cnd, &engine->rendercmd_mutex);

	// Grab a snapshot of the command->finished under the lock and then unlock
	*out_rendercmd = &engine->command_ring[engine->render_read_idx];
	b8 has_finished = (*out_rendercmd)->finished;
	mtx_unlock(&engine->rendercmd_mutex);

	return has_finished;
}

b8 engine_thread_init(box_engine* engine) {
	if (!platform_start(&engine->platform_state, &engine->config)) {
		BX_ERROR("Failed to start platform.");
		goto exit_and_cleanup;
	}

	if (!renderer_backend_create(engine->config.render_config.api_type, &engine->platform_state, &engine->renderer)
		|| !engine->renderer.initialize(&engine->renderer, engine->config.window_size, engine->config.title, &engine->config.render_config)) {
		BX_ERROR("Failed to init renderer backend.");
		goto exit_and_cleanup;
	}

	if (engine->config.target_fps == 0) {
		BX_ERROR("Cannot set box_config->target_fps to zero.");
		goto exit_and_cleanup;
	}

	const double target_frame_time = (engine->config.target_fps > 0)
		? (1000.0 / (double)engine->config.target_fps)
		: 0.0; // ms per frame

	// Send message to main thread to unlock user access to engine
	// since everthing is initialized
	{
		mtx_lock(&engine->rendercmd_mutex); // Using the same mutex as the thread waiting
		engine->is_running = TRUE;
		cnd_broadcast(&engine->rendercmd_cnd);
		mtx_unlock(&engine->rendercmd_mutex);
	}
	
	engine->last_time = platform_get_absolute_time();

	while (engine->is_running && !engine->should_quit) {
		f64 frame_start = platform_get_absolute_time();
		f64 delta_ms = frame_start - engine->last_time;
		engine->last_time = frame_start;
		engine->delta_time = delta_ms;

		resource_thread_func(&engine->resource_system);

		box_renderer_backend* backend = &engine->renderer;

		box_rendercmd* command = NULL;
		b8 has_finished = get_next_rendercmd(engine, &command);

		if (has_finished && backend->begin_frame(backend, engine->delta_time)) {
			// Grab a snapshot of the command->finished under the lock and then unlock
			if (!backend->playback_rendercmd(backend, command)) {
				BX_WARN("Could not playback render command.");
			}

			// After successful playback:
			mtx_lock(&engine->rendercmd_mutex);
			command->finished = FALSE; // Mark slot free for reuse
			// Signal any waiting writer that a slot became available
			cnd_signal(&engine->rendercmd_cnd);
			mtx_unlock(&engine->rendercmd_mutex);

			// Finish frame after sending validated rendercmd
			if (!backend->end_frame(backend)) {
				BX_ERROR("Could not finish render frame.");
				goto exit_and_cleanup;
			}
		}

		platform_pump_messages(&engine->platform_state);

		// Throttle FPS if configured (no sleep if 0 = uncapped)
		if (target_frame_time > 0.0) {
			f64 frame_end = platform_get_absolute_time();
			f64 elapsed = frame_end - frame_start;
			f64 remaining = target_frame_time - elapsed;
			if (remaining > 0.0) {
				platform_sleep(remaining);
			}
		}
	}

	cnd_broadcast(&engine->rendercmd_cnd);
	return TRUE;

exit_and_cleanup:
	BX_ERROR("Fatal error in render thread. Exiting...");

	// Broadcast to wake all waiters
	mtx_lock(&engine->rendercmd_mutex);
	engine->should_quit = TRUE;
	cnd_broadcast(&engine->rendercmd_cnd); 
	mtx_unlock(&engine->rendercmd_mutex);
	return FALSE;
}

void engine_thread_shutdown(box_engine* engine) {
	if (!engine) return;
	// Destroy in the opposite order of creation.

	BX_INFO("Destroying engine resources...");
	resource_system_destroy_resources(&engine->resource_system);

	if (engine->renderer.internal_context != NULL) {
		engine->renderer.shutdown(&engine->renderer);
		renderer_backend_destroy(&engine->renderer);
	}
	
	if (engine->platform_state.internal_state != NULL) {
		platform_shutdown(&engine->platform_state);
	}
}
