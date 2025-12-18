#include "engine.h"

#include "renderer/renderer_cmd.h"
#include "renderer/vertex_layout.h"
#include "renderer/renderer_types.h"

int main(int argc, char** argv) {
	// Sets default config for box_engine.
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
	box_vertex_layout_end(&layout);
	 
	const char* graphics_shaders[] = { "assets/shader_base.vert.spv", "assets/shader_base.frag.spv" };

	box_renderstage* renderstage = box_engine_create_renderstage(
		engine, 
		graphics_shaders, BX_ARRAYSIZE(graphics_shaders), 
		NULL, /* box_engine_create_renderbuffer(engine, BOX_RENDERBUFFER_USAGE_VERTEX, vertices, sizeof(vertices)), */
		NULL, /* box_engine_create_renderbuffer(engine, BOX_RENDERBUFFER_USAGE_INDEX, vertices, sizeof(vertices)), */
		&layout,
		FALSE, FALSE);

	box_engine_prepare_resources(engine);

	while (box_engine_is_running(engine)) {
		// Checks render thread is minimised or paused.
		if (box_engine_should_skip_frame(engine))
			continue;

		box_rendercmd* cmd = box_engine_next_rendercmd(engine);
		box_rendercmd_set_clear_colour(cmd, 0.1f, 0.1f, 0.1f);

		box_rendercmd_begin_renderstage(cmd, renderstage);
		box_rendercmd_draw(cmd, 3, 1);
		box_rendercmd_end_renderstage(cmd);

		box_engine_render_frame(engine, cmd);
	}

	box_destroy_engine(engine);
	return 0;
}