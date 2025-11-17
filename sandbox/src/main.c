#include "engine.h"

#include "voxel/octree_gen.h"
#include "renderer/vertex_layout.h"
#include "renderer/renderer_types.h"
#include "renderer/renderer_cmd.h"

int main(int argc, char** argv)
{
	box_config app_config = box_default_config();
	app_config.window_position.x = 960;
	app_config.window_position.y = 540;
	app_config.hide_until_frame = TRUE;
	box_engine* engine = box_create_engine(&app_config);

	box_octree octree;
	if (!box_load_voxel_model("assets/cars.vox", &octree)) {
		BX_ERROR("Could not load voxel model!");
		return 1;
	}

	float vertices[5 * 3] = {
		 0.0,  0.5, 1.0, 0.0, 0.0,
		 0.5, -0.5, 0.0, 1.0, 0.0,
		-0.5, -0.5, 0.0, 0.0, 1.0,
	};

	box_vertex_layout layout = { 0 };
	box_vertex_layout_add(&layout, BOX_VERTEX_ATTRIB_FLOAT32, 2); // position
	box_vertex_layout_add(&layout, BOX_VERTEX_ATTRIB_UINT8, 3); // colour
	box_vertex_layout_end(&layout);

	//box_vertexbuffer* vertex_buf = box_engine_create_vertexbuffer(engine, vertices, sizeof(vertices), &layout);

	while (box_engine_is_running(engine))
	{
		box_rendercmd* command = box_engine_next_rendercmd(engine);
		box_rendercmd_set_clear_colour(command, 0.1f, 0.1f, 0.1f);

		{
			box_rendercmd_begin_renderstage(command, "assets/shader_base.vert.spv", "assets/shader_base.frag.spv");
			//box_rendercmd_set_vertex_buffer(command, vertex_buf);

			box_rendercmd_draw(command, 3, 1);

			box_rendercmd_end_renderstage(command);
		}

		box_rendercmd_verify_resources(command, engine);
		box_engine_render_frame(engine, command);
	}

	box_destroy_voxel_model(&octree);
	box_destroy_engine(engine);
	return 0;
}