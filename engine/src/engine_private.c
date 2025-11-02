#include "defines.h"
#include "engine_private.h"

// Sleep helper
static void sleep_for_ms(long ms) {
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

	e->config.render_config.application_name = e->config.title;
	e->config.render_config.framebuffer_width = e->config.start_width;
	e->config.render_config.framebuffer_height = e->config.start_height;

	if (!e->render_state.initialize(&e->render_state, &e->config.render_config)) {
		BX_ERROR("Something went wrong with render backend, exiting...");
		return FALSE;
	}

	const double target_frame_time = (e->config.target_fps > 0)
		? (1000.0 / (double)e->config.target_fps)
		: 0.0; // ms per frame

	e->is_running = TRUE;
	e->last_time = platform_get_absolute_time();

	while (e->is_running && !e->should_quit) {
		f64 frame_start = platform_get_absolute_time();
		f64 delta_ms = frame_start - e->last_time;
		e->last_time = frame_start;

		if (backend->begin_frame(backend, e->delta_time)) {
			b8 result = backend->end_frame(backend);
			if (!result) {
				BX_ERROR("Could not finish render frame, exiting...");
				e->should_quit = TRUE;
				return FALSE;
			}
		}

		platform_pump_messages(&e->platform_state);
		e->delta_time = platform_get_absolute_time() - e->last_time;

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
