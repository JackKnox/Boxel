#include "engine.h"

#include "voxel/octree_gen.h"
#include "renderer/renderer_cmd.h"

int main(int argc, char** argv)
{
	box_config app_config = box_default_config();
	app_config.window_position.x = 960;
	app_config.window_position.y = 540;
	box_engine* engine = box_create_engine(&app_config);

	if (!engine)
	{
		BX_ERROR("Boxel initialization failed");
		return 1;
	}

	box_octree octree;
	if (!box_load_voxel_model("assets/cars.vox", &octree)) {
		BX_ERROR("Could not load voxel model!");
		return 1;
	}

	while (box_engine_is_running(engine))
	{
		box_rendercmd* command = box_engine_next_rendercmd(engine);
		box_rendercmd_set_clear_colour(command, 0.1f, 0.1f, 0.1f);

		/*
		box_rendercmd_begin_renderpass(&command
		*/

		box_engine_render_frame(engine, command);
	}

	box_destroy_voxel_model(&octree);
	box_destroy_engine(engine);
	return 0;
}