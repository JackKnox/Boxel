#include "engine.h"

#include "renderer/renderer_cmd.h"
#include "renderer/render_layout.h"
#include "renderer/renderer_types.h"

int main(int argc, char** argv) {
	box_config app_config = box_default_config();
	app_config.render_config.modes = RENDERER_MODE_COMPUTE | RENDERER_MODE_TRANSFER;

	box_engine* engine = box_create_engine(&app_config);
	if (!engine)
		BX_FATAL("Failed to init Boxel engine!");

	f32 data[] = { 1.0f, 2.0f, 3.0f, 4.0f };
	box_renderbuffer* ssbo = box_engine_create_renderbuffer(engine, BOX_RENDERBUFFER_USAGE_STORAGE, data, sizeof(data));

	box_render_layout layout = { 0 };
	box_render_layout_add_descriptor(&layout, BOX_RENDERBUFFER_USAGE_STORAGE);
	box_render_layout_end(&layout);

	const char* compute_shaders[] = { "assets/test_compute.comp.spv" };
	box_renderstage* compute_stage = box_engine_create_renderstage(engine, &layout, BX_ARRAYSIZE(compute_shaders), compute_shaders);

	box_engine_prepare_resources(engine);

	while (box_engine_is_running(engine)) {
		// Checks render thread is minimised or paused.
		if (box_engine_should_skip_frame(engine))
			continue;

		box_rendercmd* cmd = box_engine_next_rendercmd(engine);
		{
			box_rendercmd_begin_renderstage(cmd, compute_stage);
			box_rendercmd_bind_buffer(cmd, ssbo, 0, 0);

			box_rendercmd_dispatch(cmd, 1, 1, 1);

			box_rendercmd_end_renderstage(cmd);
		}
		box_engine_render_frame(engine, cmd);
	}

	box_destroy_engine(engine);
	return 0;
}