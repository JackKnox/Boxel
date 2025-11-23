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
	configuration.render_config.graphics_pipeline = TRUE;
	configuration.render_config.discrete_gpu = TRUE;
	configuration.render_config.required_extensions = darray_create(const char*);
	return configuration;
}

int render_thread_loop(void* arg) {
	if (!arg) return thrd_error;
	box_engine* engine = (box_engine*)arg; 

	if (!engine_thread_init(engine)) {
		// Error message already printed
		engine->should_quit = TRUE;
	}

	// NOTE: loop runs in init (see engine_private.c)

	engine_thread_shutdown(engine);
	engine->is_running = FALSE;

	return thrd_success; // Success
}

b8 engine_on_application_quit(u16 code, void* sender, void* listener_inst, event_context data) {
	((box_engine*)listener_inst)->should_quit = TRUE;
	return TRUE;
}

// Combines creating sunc objects into one boolean
b8 create_sync_objects(box_engine* engine) {
	if (!mtx_init(&engine->rendercmd_mutex, mtx_plain)) {
		return FALSE;
	}

	if (!cnd_init(&engine->rendercmd_cnd)) {
		return FALSE;
	}

	return TRUE;
}

// Allocates render command ring next to engine memory for speediness
box_engine* allocate_engine_and_ring(u8 ring_size) {
	u64 total_size = sizeof(box_engine) + sizeof(box_rendercmd) * ring_size;

	box_engine* engine = platform_allocate(total_size, FALSE);
	return (box_engine*)platform_zero_memory(engine, total_size);
}

box_engine* box_create_engine(box_config* app_config) {
	u8 ring_length = app_config->render_config.swapchain_frame_count + 1;
	box_engine* engine = allocate_engine_and_ring(ring_length);

	engine->command_ring = (box_rendercmd*)((u8*)engine + sizeof(box_engine));
	engine->command_ring_length = ring_length;
	engine->config = *app_config;
	//engine->is_running // Automatically do when zeroing memory...
	//	= engine->is_suspended 
	//	= engine->should_quit 
	//	= FALSE;

	if (!event_initialize()) {
		BX_ERROR("Failed to initialize event system.");
		goto failed_init;
	}

	event_register(EVENT_CODE_APPLICATION_QUIT, engine, engine_on_application_quit);

	if (!create_sync_objects(engine)) {
		BX_ERROR("Failed to create synchronization objects.");
		goto failed_init;
	}

	if (!thrd_create(&engine->render_thread, render_thread_loop, (void*)engine)) {
		BX_ERROR("Failed to start render thread.");
		goto failed_init;
	}

	WAIT_ON_CND(!engine->is_running && !engine->should_quit)
	return engine->should_quit ? NULL : engine;

failed_init:
	BX_ERROR("box_create_engine failed initialization, engine cannot continue. Exiting...");
	box_destroy_engine(engine);
	return NULL;
}

box_rendercmd* box_engine_next_rendercmd(box_engine* engine) {
	box_rendercmd_reset(&engine->command_ring[engine->game_write_idx]);
	return &engine->command_ring[engine->game_write_idx];
}

void box_engine_render_frame(box_engine* engine, box_rendercmd* command) {
	if (&engine->command_ring[engine->game_write_idx] != command) {
		BX_ERROR("Engine cannot take user-generated render commands (Use box_engine_next_rendercmd instead).");
		return;
	}

	mtx_lock(&engine->rendercmd_mutex);

	// Signal first render command ever sent
	if (!engine->first_cmd_sent) {
		engine->first_cmd_sent = TRUE;
		cnd_broadcast(&engine->rendercmd_cnd);
	}

	engine->game_write_idx = (engine->game_write_idx + 1) % engine->command_ring_length;

	// If advancing write_idx would equal read_idx, wait until reader consumes
	while (engine->render_read_idx == engine->game_write_idx && engine->is_running && !engine->should_quit)
		cnd_wait(&engine->rendercmd_cnd, &engine->rendercmd_mutex);

	command->finished = TRUE;

	// Notify the render thread there is something to play back
	cnd_signal(&engine->rendercmd_cnd);
	mtx_unlock(&engine->rendercmd_mutex);
}

const box_config* box_engine_get_config(box_engine* engine) {
	if (!engine) return FALSE;
	return &engine->config;
}

b8 box_engine_is_running(box_engine* engine) {
	if (!engine) return FALSE;
	return engine->is_running;
}

void box_destroy_engine(box_engine* engine) {
	if (!engine) return;
	// Destroy in the opposite order of creation.

	if (engine->is_running) { // Error if joining when thread has alreading exited...
		int success_code = thrd_success;
		thrd_join(engine->render_thread, NULL);

		if (engine->is_running || success_code != thrd_success) {
			BX_WARN("Render thread closed unexpectedly...");
		}
	}

	// Destroy sync objects...
	mtx_destroy(&engine->rendercmd_mutex);
	cnd_destroy(&engine->rendercmd_cnd);

	event_shutdown();

	platform_free(engine, FALSE);
}