#include "defines.h"
#include "engine.h"

#include "renderer/renderer_backend.h"

#include "engine_private.h"
#include "utils/darray.h"

box_config box_default_config() {
	box_config configuration = { 0 }; // fill with zeros
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
	box_engine* engine = (box_engine*)arg;
	BX_ASSERT(engine != NULL && "Invalid box_engine passed to thread argument");

	if (!engine_thread_init(engine)) {
		// Error message already printed
		engine->should_quit = TRUE;
	}

	// NOTE: loop runs in init (see engine_private.c)

	engine_thread_shutdown(engine);

	engine->is_running = FALSE;
	engine->should_quit = FALSE;
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
	if (!app_config || app_config->target_fps <= 0 || !app_config->title || app_config->render_config.modes == 0 || app_config->render_config.frames_in_flight <= 1) {
		BX_ERROR("Invalid configuration passed to box_create_engine"); 
		return NULL; 
	}

	// Init core systems
	if (!event_initialize()) {
		BX_ERROR("Failed to initialize core event system.");
		return NULL;
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

	if (!box_resource_system_init(&engine->resource_system, 1024)) {
		BX_ERROR("Failed to initialize resource system.");
		goto failed_init;
	}

	event_register(EVENT_CODE_APPLICATION_QUIT, engine, engine_on_application_quit);

	if (!cnd_init(&engine->rendercmd_cnd) ||
		!mtx_init(&engine->rendercmd_mutex, BOX_MUTEX_TYPE_PLAIN) ||
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

b8 box_engine_is_running(box_engine* engine) {
	if (!engine) return FALSE;
	return engine->is_running;
}

b8 box_engine_should_skip_frame(box_engine* engine) {
	if (!engine) return FALSE;
	return engine->is_suspended;
}

void box_close_engine(box_engine* engine, b8 should_close) {
	if (!engine || !should_close || !engine->is_running) return;
	BX_TRACE("Closing window...");
	engine->should_quit = TRUE;
}

box_resource_system* box_engine_get_resource_system(box_engine* engine) {
	if (!engine) return NULL;
	return &engine->resource_system;
}

void box_engine_prepare_resources(box_engine* engine) {
	if (!engine) return;
	box_resource_system_flush_uploads(&engine->resource_system);
}

const box_config* box_engine_get_config(box_engine* engine) {
	if (!engine) return NULL;
	return &engine->config;
}

box_rendercmd* box_engine_next_frame(box_engine* engine) {
	if (!engine) return NULL;
	mtx_lock(&engine->rendercmd_mutex);

	// Finish previous frame (if any)
	if (engine->has_open_frame) {
		box_rendercmd* prev = &engine->command_ring[engine->game_write_idx];
		_box_rendercmd_end(prev);

		engine->game_write_idx = (engine->game_write_idx + 1) % engine->command_ring_length;
		engine->has_open_frame = FALSE;

		cnd_signal(&engine->rendercmd_cnd);
	}

	while (((engine->game_write_idx + 1) % engine->command_ring_length) == engine->render_read_idx 
		&& engine->is_running && !engine->should_quit) {
		// Wait for free slot
		cnd_wait(&engine->rendercmd_cnd, &engine->rendercmd_mutex);
	}

	// Begin new frame
	box_rendercmd* cmd = &engine->command_ring[engine->game_write_idx];
	box_rendercmd_reset(cmd);

	engine->has_open_frame = TRUE;

	mtx_unlock(&engine->rendercmd_mutex);
	return cmd;
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

	BX_TRACE("Freeing command buffer ring...");
	for (int i = 0; i < engine->command_ring_length; ++i) {
		box_rendercmd_destroy(&engine->command_ring[i]);
	}

	engine->command_ring_length = 0;

	bfree(engine, engine->allocation_size, MEMORY_TAG_ENGINE);

	event_shutdown();
	memory_shutdown();
}