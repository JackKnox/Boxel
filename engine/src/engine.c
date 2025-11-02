#include "defines.h"
#include "engine.h"

#include "platform/platform.h"
#include "renderer/renderer_backend.h"

#include "engine_private.h"
#include "utils/darray.h"

box_config box_default_config() {
	box_config configuration = {0}; // fill with zeros
	configuration.start_pos_x = 0;
	configuration.start_pos_y = 0;
	configuration.start_width = 640;
	configuration.start_height = 360;
	configuration.title = "Boxel Sandbox";
	configuration.target_fps = 60;

	configuration.render_config.api_type = RENDERER_BACKEND_TYPE_VULKAN;
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
	box_engine* e = (box_engine*)platform_allocate(sizeof(box_engine), FALSE);    
	platform_zero_memory(e, sizeof(box_engine));

	e->is_running = FALSE;
	e->should_quit = FALSE;
	e->config = *app_config;

	if (!event_initialize()) {
		BX_ERROR("Event system failed initialization. Engine cannot continue.");
		return FALSE;
	}

	event_register(EVENT_CODE_APPLICATION_QUIT, e, engine_on_application_quit);

	if (thrd_create(&e->render_thread, render_thread_loop, e) != thrd_success) {
		// Couldn't start render thread, exiting...
		box_destroy_engine(e);
		return NULL;
	}

	while (!e->is_running && !e->should_quit) {  }
	return e->should_quit ? NULL : e;
}

b8 box_engine_is_running(box_engine* engine) {
	return engine->is_running;
}

const box_config* box_engine_get_config(box_engine* engine) {
	return &engine->config;
}

void box_engine_render_frame(box_engine* engine, box_rendercmd* command) {
	engine->command = *command;
}

void box_destroy_engine(box_engine* engine) {
	if (!engine) return;

	engine->should_quit = TRUE;
	if (engine->is_running) {
		thrd_join(engine->render_thread, NULL);
	}

	event_shutdown();
	platform_free(engine, FALSE);
}