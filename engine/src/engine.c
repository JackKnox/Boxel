#include "defines.h"
#include "engine.h"

#include "renderer/renderer_backend.h"

#include "engine_private.h"
#include "utils/darray.h"

box_config box_default_config() {
	box_config configuration = {0}; // fill with zeros
	configuration.window_size.x = 640;
	configuration.window_size.y = 360;
	configuration.title = "Boxel Sandbox";
	configuration.target_fps = 60;
	configuration.hide_until_frame = FALSE;

	configuration.render_config.api_type = RENDERER_BACKEND_TYPE_VULKAN;
	configuration.render_config.swapchain_frame_count = 2;
	configuration.render_config.enable_validation = TRUE;
	configuration.render_config.graphics = TRUE;
	configuration.render_config.present = TRUE;
	configuration.render_config.discrete_gpu = TRUE;
	configuration.render_config.required_extensions = darray_create(const char*);
	return configuration;
}

int render_thread_loop(void* arg) {
	if (!arg) return thrd_error;
	box_engine* e = (box_engine*)arg; 

	if (!engine_thread_init(e)) {
		BX_ERROR("Could not init engine thread, exiting...");
		e->should_quit = TRUE;
	}

	// NOTE: loop runs in init (see engine_private.c)

	e->is_running = FALSE;
	engine_thread_shutdown(e);

	return thrd_success; // Success
}

b8 engine_on_application_quit(u16 code, void* sender, void* listener_inst, event_context data) {
	if (code == EVENT_CODE_APPLICATION_QUIT) {
		box_engine* e = (box_engine*)listener_inst;
		e->should_quit = TRUE;

		return TRUE;
	}

	return FALSE;
}

box_engine* box_create_engine(box_config* app_config) {
	box_engine* engine = (box_engine*)platform_allocate(sizeof(box_engine), FALSE);    
	platform_zero_memory(engine, sizeof(box_engine));

	engine->is_running = FALSE;
	engine->should_quit = FALSE;
	engine->config = *app_config;

	if (!event_initialize()) {
		BX_ERROR("Event system failed initialization. Engine cannot continue.");
		return FALSE;
	}

	event_register(EVENT_CODE_APPLICATION_QUIT, engine, engine_on_application_quit);

	engine->command_queue = darray_reserve(box_rendercmd, engine->config.render_config.swapchain_frame_count);
	mtx_init(&engine->rendercmd_mutex, mtx_plain);
	cnd_init(&engine->rendercmd_cv);

	if (thrd_create(&engine->render_thread, render_thread_loop, engine) != thrd_success) {
		// Couldn't start render thread, exiting...
		box_destroy_engine(engine);
		return NULL;
	}

	while (!engine->is_running && !engine->should_quit) {  }
	return engine->should_quit ? NULL : engine;
}

b8 box_engine_is_running(box_engine* engine) {
	return engine->is_running;
}

const box_config* box_engine_get_config(box_engine* engine) {
	return &engine->config;
}

box_rendercmd* box_engine_next_rendercmd(box_engine* engine) {
	box_rendercmd_reset(&engine->command_queue[engine->game_write_idx]);
	return &engine->command_queue[engine->game_write_idx];
}

void box_engine_render_frame(box_engine* engine, box_rendercmd* command) {
	if (&engine->command_queue[engine->game_write_idx] != command) {
		BX_ERROR("Engine cannot take user-generated rendercmd (Use box_engine_next_rendercmd).");
		return;
	}

	mtx_lock(&engine->rendercmd_mutex);

	engine->frame_ready = TRUE;
	engine->game_write_idx = (engine->game_write_idx + 1) % darray_capacity(engine->command_queue);
	cnd_signal(&engine->rendercmd_cv);
	mtx_unlock(&engine->rendercmd_mutex);
}

void box_destroy_engine(box_engine* engine) {
	if (!engine) return;

	engine->should_quit = TRUE;
	if (engine->is_running) {
		thrd_join(engine->render_thread, NULL);
	}

	event_shutdown();

	darray_destroy(engine->command_queue);
	mtx_destroy(&engine->rendercmd_mutex);
	cnd_destroy(&engine->rendercmd_cv);

	darray_destroy(engine->config.render_config.required_extensions);
	platform_free(engine, FALSE);
}