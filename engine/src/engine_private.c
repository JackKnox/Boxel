#include "defines.h"
#include "engine_private.h"

#include "utils/darray.h"

// NOTE: Engine thread should never call functions like box_destroy_engine
//       which deallocates memory that main thread uses. When finding a fatal error
//       use "engine->should_quit = TRUE" and return to notify main thread to destroy
//       at a time which is safe for the program (see tag "exit_and_cleanup").

box_rendercmd* get_next_rendercmd(box_engine* engine) {
	// Get next read index reliably
	mtx_lock(&engine->rendercmd_mutex);

	// Wait until the writer has written to that slot (i.e., not equal to write index).
	while (engine->render_read_idx == engine->game_write_idx && 
		engine->is_running && !engine->should_quit) {
		cnd_wait(&engine->rendercmd_cnd, &engine->rendercmd_mutex);
	}

	engine->render_read_idx = (engine->render_read_idx + 1) % engine->command_ring_length;

	// Grab a snapshot of the command under the lock and then unlock
	box_rendercmd* rendercmd = &engine->command_ring[engine->render_read_idx];
	mtx_unlock(&engine->rendercmd_mutex);

	return rendercmd;
}

b8 playback_rendercmd(box_engine* engine, box_rendercmd* rendercmd) {
	box_rendercmd_context context = { 0 };

	u8* cursor = 0;
	while (freelist_next_block(&rendercmd->buffer, &cursor)) {
		rendercmd_header* hdr = (rendercmd_header*)cursor;
		rendercmd_payload* payload = (rendercmd_payload*)(cursor + sizeof(rendercmd_header));

		if (hdr->supported_mode != 0) {
			context.current_mode = hdr->supported_mode;
		}

#if BOX_ENABLE_VALIDATION
		if (hdr->supported_mode != 0) {
			if ((engine->config.render_config.modes & hdr->supported_mode) == 0) {
				BX_ERROR("Render command in box_rendercmd isn't supported by renderer configuration");
				return FALSE;
			}
		}
#endif

		switch (hdr->type) {
		case RENDERCMD_BEGIN_RENDERSTAGE:
#if BOX_ENABLE_VALIDATION
			if (context.current_shader != NULL) {
				BX_ERROR("Tried to begin renderstage twice in box_rendercmd.");
				return FALSE;
			}
#endif

			context.current_shader = payload->begin_renderstage.renderstage;
			break;

		case RENDERCMD_END_RENDERSTAGE:
#if BOX_ENABLE_VALIDATION
			if (context.current_shader == NULL) {
				BX_ERROR("Tried to end renderstage twice in box_rendercmd.");
				return FALSE;
			}
#endif

			break;

		case RENDERCMD_SET_DESCRIPTOR:
		case RENDERCMD_DRAW:
		case RENDERCMD_DRAW_INDEXED:
		case RENDERCMD_DISPATCH:
#if BOX_ENABLE_VALIDATION
			if (context.current_shader == NULL) {
				BX_ERROR("Tried to dispatch draw call without a renderstage in box_rendercmd.");
				return FALSE;
			}
#endif
			break;
		}

		engine->renderer.playback_rendercmd(&engine->renderer, &context, hdr, payload);
	}

	return TRUE;
}

b8 engine_thread_init(box_engine* engine) {
	if (!platform_start(&engine->platform_state, &engine->config)) {
		BX_ERROR("Failed to start platform.");
		goto exit_and_cleanup;
	}

	if (!renderer_backend_create(&engine->config.render_config, engine->config.window_size, engine->config.title, &engine->platform_state, &engine->renderer)) {
		BX_ERROR("Failed to init renderer backend.");
		goto exit_and_cleanup;
	}

	if (engine->config.target_fps == 0) {
		BX_ERROR("Cannot set box_config->target_fps to zero.");
		goto exit_and_cleanup;
	}

	const f64 target_frame_time = (engine->config.target_fps > 0)
		? (1000.0 / (f64)engine->config.target_fps)
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

		box_renderer_backend* backend = &engine->renderer;
		box_rendercmd* command = get_next_rendercmd(engine);

		if (command != NULL && backend->begin_frame(backend, engine->delta_time)) {
			b8 succeceded = playback_rendercmd(engine, command);
			if (!succeceded) {
				BX_WARN("Could not playback render command.");
			}

			// After successful playback:
			mtx_lock(&engine->rendercmd_mutex);

			command->finished = FALSE; // Mark slot free for reuse
			cnd_signal(&engine->rendercmd_cnd);

			mtx_unlock(&engine->rendercmd_mutex);

			// Finish frame after sending validated rendercmd
			if (succeceded && !backend->end_frame(backend)) {
				BX_ERROR("Could not finish render frame.");
				goto exit_and_cleanup;
			}
		}

		platform_pump_messages(&engine->platform_state);

		// Throttle FPS if configured
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

	engine->renderer.wait_until_idle(&engine->renderer, UINT64_MAX);

	BX_INFO("Destroying engine resources...");
	resource_system_shutdown(&engine->resource_system);

	BX_INFO("Shutting down renderer backend...");
	if (engine->renderer.internal_context != NULL) {
		renderer_backend_destroy(&engine->renderer);
	}

	BX_INFO("Close connection to operating system...");
	if (engine->platform_state.internal_state != NULL) {
		platform_shutdown(&engine->platform_state);
	}
}
