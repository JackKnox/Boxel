#include "defines.h"
#include "engine_private.h"

#include "utils/darray.h"

// NOTE: Engine thread should never call functions like box_destroy_engine
//       which deallocates memory that main thread uses. When finding a fatal error
//       use "engine->should_quit = TRUE" and return to notify main thread to destroy
//       at a time which is safe for the program (see tag "exit_and_cleanup").

box_rendercmd* get_next_rendercmd(box_engine* engine) {
	// Get next read index reliably
	mutex_lock(&engine->rendercmd_mutex);

	// Wait until the writer has written to that slot (i.e., not equal to write index).
	while (engine->render_read_idx == engine->game_write_idx && 
		engine->is_running && !engine->should_quit) {
		cond_wait(&engine->rendercmd_cnd, &engine->rendercmd_mutex);
	}

	engine->render_read_idx = (engine->render_read_idx + 1) % engine->command_ring_length;

	// Grab a snapshot of the command under the lock and then unlock
	box_rendercmd* rendercmd = &engine->command_ring[engine->render_read_idx];
	mutex_unlock(&engine->rendercmd_mutex);

	return rendercmd;
}

b8 engine_thread_init(box_engine* engine) {
	if (!platform_start(&engine->platform_state, &engine->config)) {
		BX_ERROR("Failed to start platform.");
		goto exit_and_cleanup;
	}

	if (!box_renderer_backend_create(&engine->config.render_config, engine->config.window_size, engine->config.title, &engine->platform_state, &engine->renderer)) {
		BX_ERROR("Failed to init renderer backend.");
		goto exit_and_cleanup;
	}

	const f64 target_frame_time = (engine->config.target_fps > 0)
		? (1000.0 / (f64)engine->config.target_fps)
		: 0.0; // ms per frame

	// Send message to main thread to unlock user access to engine
	// since everthing is initialized
	{
		mutex_lock(&engine->rendercmd_mutex); // Using the same mutex as the thread waiting
		engine->is_running = TRUE;
		cond_broadcast(&engine->rendercmd_cnd);
		mutex_unlock(&engine->rendercmd_mutex);
	}
	
	engine->last_time = platform_get_absolute_time();

	while (engine->is_running && !engine->should_quit) {
		f64 frame_start = platform_get_absolute_time();
		f64 delta_ms = frame_start - engine->last_time;
		engine->last_time = frame_start;
		engine->delta_time = delta_ms;

		box_renderer_backend* backend = &engine->renderer;
		box_rendercmd* command = get_next_rendercmd(engine);

		if (command != NULL && backend->begin_frame(backend, engine->delta_time)) {
			bzero_memory(&engine->command_context, sizeof(box_rendercmd_context));

			if (box_renderer_backend_submit_rendercmd(backend, &engine->command_context, command)) {
				// After successful playback:
				mutex_lock(&engine->rendercmd_mutex);
				command->finished = FALSE; // Mark slot free for reuse

				cond_signal(&engine->rendercmd_cnd);
				mutex_unlock(&engine->rendercmd_mutex);

				// Finish frame after sending validated rendercmd
				if (!backend->end_frame(backend)) {
					BX_ERROR("Could not finish render frame.");
					goto exit_and_cleanup;
				}
			}
			else {
				BX_WARN("Could not playback render command.");
			}
		}

		platform_pump_messages(&engine->platform_state);

		// Throttle FPS if configured
		if (target_frame_time > 0.0) {
			f64 frame_end = platform_get_absolute_time();
			f64 elapsed = frame_end - frame_start;
			f64 remaining = target_frame_time - elapsed;
			
			if (remaining > 0.0)
				platform_sleep(remaining);
		}
	}

	cond_broadcast(&engine->rendercmd_cnd);
	return TRUE;

exit_and_cleanup:
	BX_ERROR("Fatal error in render thread. Exiting...");

	// Broadcast to wake all waiters
	mutex_lock(&engine->rendercmd_mutex);
	engine->should_quit = TRUE;
	cond_broadcast(&engine->rendercmd_cnd); 
	mutex_unlock(&engine->rendercmd_mutex);
	return FALSE;
}

void engine_thread_shutdown(box_engine* engine) {
	if (!engine) return;
	// Destroy in the opposite order of creation.

	if (engine->renderer.internal_context != NULL) {
		engine->renderer.wait_until_idle(&engine->renderer, UINT64_MAX);
	}

	BX_INFO("Destroying engine resources...");
	resource_system_shutdown(engine->resource_system);

	BX_INFO("Shutting down renderer backend...");
	if (engine->renderer.internal_context != NULL) {
		box_renderer_backend_destroy(&engine->renderer);
	}

	BX_INFO("Close connection to operating system...");
	if (engine->platform_state.internal_state != NULL) {
		platform_shutdown(&engine->platform_state);
	}
}
