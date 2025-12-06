#include "engine.h"

#include "voxel/octree_gen.h"
#include "renderer/renderer_cmd.h"
#include "renderer/vertex_layout.h"
#include "renderer/renderer_types.h"

int main(int argc, char** argv)
{
	box_config app_config = box_default_config();
	box_engine* engine = box_create_engine(&app_config);

	if (engine == NULL)
		BX_FATAL("Failed to init Boxel engine!");

	f32 vertices[] = {
		 0.0,  0.5, 1.0, 0.0, 0.0,
		 0.5, -0.5, 0.0, 1.0, 0.0,
		-0.5, -0.5, 0.0, 0.0, 1.0,
	};

	box_vertex_layout layout = {0};
	box_vertex_layout_set_topology(&layout, BOX_VERTEX_TOPOLOGY_TRIANGLES); // Triangle list
	box_vertex_layout_add(&layout, BOX_VERTEX_ATTRIB_FLOAT32, 2); // Position
	box_vertex_layout_add(&layout, BOX_VERTEX_ATTRIB_FLOAT32, 3); // Colour
	 
	const char* graphics_shaders[] = { "assets/shader_base.vert.spv", "assets/shader_base.frag.spv" };
	box_shader* shader = box_engine_create_shader(engine, graphics_shaders, BX_ARRAYSIZE(graphics_shaders));

	while (box_engine_is_running(engine))
	{
		if (box_engine_should_skip_frame(engine)) {
			continue;
		}

		box_rendercmd* cmd = box_engine_next_rendercmd(engine);
		box_rendercmd_set_clear_colour(cmd, 0.1f, 0.1f, 0.1f);

		/*
		box_rendercmd_begin_renderstage(cmd, shader, &layout, NULL, NULL, TRUE, FALSE);
		box_rendercmd_draw(cmd, 3, 1);
		box_rendercmd_end_renderstage(cmd);
		*/

		box_engine_render_frame(engine, cmd);
	}

	box_destroy_engine(engine);
	return 0;
}