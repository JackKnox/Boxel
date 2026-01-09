#include "engine.h"

#include "renderer/renderer_cmd.h"
#include "renderer/render_layout.h"
#include "renderer/renderer_types.h"

#include "voxel_model.h"

int main(int argc, char** argv) {
	// Sets default config for box_engine.
	box_config app_config = box_default_config();
	app_config.render_config.modes = RENDERER_MODE_GRAPHICS;
	app_config.output_diagnostics = FALSE;

	box_engine* engine = box_create_engine(&app_config);
	if (engine == NULL)
		BX_FATAL("Failed to init Boxel engine!");

	voxel_model* model = voxel_model_create(engine, "assets/cars.vox");

	box_engine_prepare_resources(engine);

	box_engine_output_diagnostics(engine);

	while (box_engine_is_running(engine)) {
		// Checks render thread is minimised or paused.
		if (box_engine_should_skip_frame(engine))
			continue;

		box_rendercmd* cmd = box_engine_next_frame(engine);
		{
			box_rendercmd_set_clear_colour(cmd, 0.1f, 0.1f, 0.1f);
		}
		box_engine_end_frame(engine);
	}

	box_destroy_engine(engine);
	return 0;
}