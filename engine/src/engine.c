#include "defines.h"
#include "engine.h"

#include "renderer/renderer_backend.h"

#include "engine_private.h"
#include "utils/darray.h"

box_config box_default_config() {
	box_config configuration = {0}; // fill with zeros
	configuration.window_mode = BOX_WINDOW_MODE_WINDOWED;
	configuration.window_position.centered = TRUE;
	configuration.window_size.x = 640;
	configuration.window_size.y = 480;
	configuration.title = "Boxel Sandbox";
	configuration.target_fps = 60;

	configuration.render_config = renderer_backend_default_config();
	configuration.render_config.api_type = RENDERER_BACKEND_TYPE_VULKAN;
	configuration.render_config.discrete_gpu = TRUE;
	return configuration;
}

b8 render_thread_loop(void* arg) {
	if (!arg) return FALSE;
	box_engine* engine = (box_engine*)arg; 

	if (!engine_thread_init(engine)) {
		// Error message already printed
		engine->should_quit = TRUE;
	}

	// NOTE: loop runs in init (see engine_private.c)

	engine_thread_shutdown(engine);
	engine->is_running = FALSE;

	return TRUE; // Success
}

b8 engine_on_application_quit(u16 code, void* sender, void* listener_inst, event_context data) {
	box_close_engine((box_engine*)listener_inst, TRUE);
	return FALSE;
}

// Allocates render command ring next to engine memory for speediness
box_engine* allocate_engine_and_ring(u32 ring_size) {
	u64 total_size = sizeof(box_engine) + sizeof(box_rendercmd) * ring_size;
	box_engine* engine = ballocate(total_size, MEMORY_TAG_ENGINE);

	engine->allocation_size = total_size;
	return engine;
}

box_engine* box_create_engine(box_config* app_config) {
	if (!app_config) return NULL;

	// Init core systems
	if (!memory_initialize()) {
		BX_ERROR("Failed to initialize memory system.");
		goto failed_init;
	}

	if (!event_initialize()) {
		BX_ERROR("Failed to initialize event system.");
		goto failed_init;
	}

	u32 ring_length = (u32)app_config->render_config.frames_in_flight + 1;
	box_engine* engine = allocate_engine_and_ring(ring_length);
	if (!engine) {
		BX_ERROR("Failed to allocate engine memory.");
		goto failed_init;
	}

	engine->command_ring = (box_rendercmd*)((u8*)engine + sizeof(box_engine));
	engine->command_ring_length = (u32)ring_length;
	engine->config = *app_config;

	if (!resource_system_init(&engine->resource_system, 1024)) {
		BX_ERROR("Failed to initialize resource system.");
		goto failed_init;
	}

	event_register(EVENT_CODE_APPLICATION_QUIT, engine, engine_on_application_quit);

	if (!cnd_init(&engine->rendercmd_cnd) ||
		!mtx_init(&engine->rendercmd_mutex, mtx_plain) ||
		!thrd_create(&engine->render_thread, render_thread_loop, (void*)engine)) {
		BX_ERROR("Failed to start render thread.");
		goto failed_init;
	}

	// Wait for the render thread to signal startup
	mtx_lock(&engine->rendercmd_mutex);
	while (!engine->is_running && !engine->should_quit)
		cnd_wait(&engine->rendercmd_cnd, &engine->rendercmd_mutex);

	b8 failed = engine->should_quit;
	mtx_unlock(&engine->rendercmd_mutex);

	if (failed) {
		BX_ERROR("Failed to initialize render thread.");
		goto failed_init;
	}

	print_memory_usage();
	return engine;

failed_init:
	BX_ERROR("box_create_engine failed initialization, engine cannot continue. Exiting...");
	box_destroy_engine(engine);
	return NULL;
}

box_rendercmd* box_engine_next_rendercmd(box_engine* engine) {
	if (!engine) return NULL;
	box_rendercmd_reset(&engine->command_ring[engine->game_write_idx]);

	return &engine->command_ring[engine->game_write_idx];
}

void box_engine_render_frame(box_engine* engine, box_rendercmd* command) {
	if (&engine->command_ring[engine->game_write_idx] != command) {
		BX_ERROR("Engine cannot take user-generated render commands (Use box_engine_next_rendercmd instead).");
		return;
	}

	mtx_lock(&engine->rendercmd_mutex);

	// If advancing write_idx would equal read_idx, wait until reader consumes
	while (engine->render_read_idx == engine->game_write_idx && engine->is_running && !engine->should_quit)
		cnd_wait(&engine->rendercmd_cnd, &engine->rendercmd_mutex);

	engine->game_write_idx = (engine->game_write_idx + 1) % engine->command_ring_length;
	command->finished = TRUE;

	// Notify the render thread there is something to play back
	cnd_signal(&engine->rendercmd_cnd);
	mtx_unlock(&engine->rendercmd_mutex);
}

const box_config* box_engine_get_config(box_engine* engine) {
	if (!engine) return NULL;
	return &engine->config;
}

b8 box_engine_is_running(box_engine* engine) {
	if (!engine) return FALSE;
	return engine->is_running;
}

b8 box_engine_should_skip_frame(box_engine* engine) {
	if (!engine) return TRUE;
	return engine->is_suspended;
}

void box_close_engine(box_engine* engine, b8 should_close) {
	if (!engine || !should_close || engine->should_quit) return;
	BX_TRACE("Closing window...");
	engine->should_quit = TRUE;
}

box_resource_system* box_engine_get_resource_system(box_engine* engine) {
	if (!engine) return NULL;
	return &engine->resource_system;
}

void box_engine_prepare_resources(box_engine* engine) {
	if (!engine) return;
	resource_system_wait(&engine->resource_system);
}

void box_destroy_engine(box_engine* engine) {
	if (!engine) return;
	// Destroy in the opposite order of creation.

	box_close_engine(engine, TRUE);

	if (engine->render_thread != NULL) {
		int success_code = TRUE;
		thrd_join(engine->render_thread, &success_code);

		if (engine->is_running || success_code != TRUE) {
			BX_WARN("Render thread closed unexpectedly...");
		}
	}

	// Destroy sync objects...
	mtx_destroy(&engine->rendercmd_mutex);
	cnd_destroy(&engine->rendercmd_cnd);

	event_shutdown();

	BX_TRACE("Freeing command buffer ring...");
	for (int i = 0; i < engine->command_ring_length; ++i) {
		box_rendercmd_destroy(&engine->command_ring[i]);
	}

	bfree(engine, engine->allocation_size, MEMORY_TAG_ENGINE);
	memory_shutdown();
}