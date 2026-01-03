#include "engine.h"

#include "renderer/renderer_cmd.h"
#include "renderer/render_layout.h"
#include "renderer/renderer_types.h"

int main(int argc, char** argv) {
	// Sets default config for box_engine.
	box_config app_config = box_default_config();

	box_engine* engine = box_create_engine(&app_config);
	if (engine == NULL)
		BX_FATAL("Failed to init Boxel engine!");

	box_engine_prepare_resources(engine);

	while (box_engine_is_running(engine)) {
		// Checks render thread is minimised or paused.
		if (box_engine_should_skip_frame(engine))
			continue;

		box_rendercmd* cmd = box_engine_next_frame(engine);
		{
			box_rendercmd_set_clear_colour(cmd, 0.1f, 0.1f, 0.1f);
		}
	}

	box_destroy_engine(engine);
	return 0;
}