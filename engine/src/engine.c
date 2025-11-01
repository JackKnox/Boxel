#include "defines.h"
#include "engine.h"

#include "platform/platform.h"
#include "renderer/renderer_backend.h"

typedef struct box_engine {
	b8 is_running;
	b8 should_quit; // TODO: atomic_flag quit_signal
	box_config config; // Will change to affect the current state
	box_platform platform_state;

	renderer_backend render_state;
	thrd_t render_thread;

	f64 last_time;
	f64 delta_time;
} box_engine;

box_config box_default_config() {
	box_config configuration = {0}; // fill with zeros
	configuration.start_pos_x = 0;
	configuration.start_pos_y = 0;
	configuration.start_width = 640;
	configuration.start_height = 360;
	configuration.title = "Boxel Sandbox";
	configuration.target_fps = 60;
	configuration.enable_validation = TRUE;

	return configuration;
}

// Sleep helper
static void sleep_for_ms(long ms) {
	if (ms <= 0) return;
	struct timespec ts;
	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (ms % 1000) * 1000000L;
	thrd_sleep(&ts, NULL);
}

int render_thread_loop(void* arg) {
	if (!arg) return thrd_error;
	box_engine* e = (box_engine*)arg;

	renderer_backend* backend = &e->render_state;
	if (!backend->initialize(backend, &e->config)) {
		return thrd_error;
	}

	const double target_frame_time = (e->config.target_fps > 0)
		? (1000.0 / (double)e->config.target_fps)
		: 0.0; // ms per frame

	e->last_time = platform_get_absolute_time();

	while (e->is_running && !e->should_quit) {
		f64 frame_start = platform_get_absolute_time();
		f64 delta_ms = frame_start - e->last_time;
		e->last_time = frame_start;

		if (backend->begin_frame(backend, e->delta_time)) {
			b8 result = backend->end_frame(backend);
			if (!result) {
				// Unrecoverable error from renderer end_frame: request shutdown.
				e->should_quit = TRUE;
				break;
			}
		}
		
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

	backend->shutdown(&e->render_state);
	e->is_running = FALSE;

	return thrd_success; // Success
}

box_engine* box_create_engine(box_config* app_config) {
	box_engine* e = (box_engine*)platform_allocate(sizeof(box_engine), FALSE);    
	platform_zero_memory(e, sizeof(box_engine));

	e->is_running = FALSE;
	e->should_quit = FALSE;
	e->config = *app_config;

	if (!platform_start(&e->platform_state, &e->config)) {
		// Something went wrong with platform, exiting...
		box_destroy_engine(e);
		return NULL;
	}

	// TODO: Make backend type configurable
	if (!renderer_backend_create(RENDERER_BACKEND_TYPE_VULKAN, &e->render_state)) {
		// Something went wrong with renderer backend, exiting...
		box_destroy_engine(e);
		return NULL;
	}

	e->is_running = TRUE;
	if (thrd_create(&e->render_thread, render_thread_loop, e) != thrd_success) {
		// Couldn't start render thread, exiting...
		e->is_running = FALSE;
		box_destroy_engine(e);
		return NULL;
	}

	return e;
}

b8 box_engine_is_running(box_engine* engine) {
	return engine->is_running;
}

const box_config* box_engine_get_config(box_engine* engine) {
	return &engine->config;
}

void box_destroy_engine(box_engine* engine) {
	if (!engine) return;

	engine->should_quit = TRUE;
	if (engine->is_running) {
		thrd_join(engine->render_thread, NULL);
	}

	renderer_backend_destroy(&engine->render_state);

	platform_shutdown(&engine->platform_state);
	platform_free(engine, FALSE);
}