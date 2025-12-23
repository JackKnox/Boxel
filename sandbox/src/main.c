#include "engine.h"

#include "renderer/renderer_cmd.h"
#include "renderer/vertex_layout.h"
#include "renderer/renderer_types.h"

int main(int argc, char** argv) {
	// Sets default config for box_engine.
	box_config app_config = box_default_config();
	app_config.render_config.modes = RENDERER_MODE_GRAPHICS  | RENDERER_MODE_TRANSFER;
	box_engine* engine = box_create_engine(&app_config);

	if (engine == NULL)
		BX_FATAL("Failed to init Boxel engine!");

	f32 vertices[] = {
		-0.5f, -0.5f, 0.15f, 0.80f, 0.90f,
		 0.5f, -0.5f, 0.20f, 0.90f, 0.60f,
		 0.5f,  0.5f, 0.40f, 0.65f, 0.95f,
		-0.5f,  0.5f, 0.80f, 0.85f, 0.95f 
	};

	u16 indices[] = { 0, 1, 2, 2, 3, 0 };

	box_vertex_layout layout = {0};
	box_vertex_layout_set_topology(&layout, BOX_VERTEX_TOPOLOGY_TRIANGLES); // Triangle list
	box_vertex_layout_set_index_type(&layout, BOX_VERTEX_ATTRIB_UINT16);
	box_vertex_layout_add(&layout, BOX_VERTEX_ATTRIB_FLOAT32, 2); // Position
	box_vertex_layout_add(&layout, BOX_VERTEX_ATTRIB_FLOAT32, 3); // Colour
	box_vertex_layout_end(&layout);
	 
	const char* graphics_shaders[] = { "assets/shader_base.vert.spv", "assets/shader_base.frag.spv" };

	box_renderbuffer* vert_buffer = box_engine_create_renderbuffer(engine, BOX_RENDERBUFFER_USAGE_VERTEX, vertices, sizeof(vertices));
	box_renderbuffer* index_buffer = box_engine_create_renderbuffer(engine, BOX_RENDERBUFFER_USAGE_INDEX, indices, sizeof(indices));

	box_renderstage* renderstage = box_engine_create_renderstage(
		engine, 
		graphics_shaders, BX_ARRAYSIZE(graphics_shaders), 
		&layout,
		FALSE, TRUE);

	box_engine_prepare_resources(engine);

	while (box_engine_is_running(engine)) {
		// Checks render thread is minimised or paused.
		if (box_engine_should_skip_frame(engine))
			continue;

		box_rendercmd* cmd = box_engine_next_rendercmd(engine);
		box_rendercmd_set_clear_colour(cmd, 0.1f, 0.1f, 0.1f);

		box_rendercmd_begin_renderstage(cmd, renderstage);
		box_rendercmd_bind_buffer(cmd, vert_buffer, 0, 0);
		box_rendercmd_bind_buffer(cmd, index_buffer, 0, 0);

		box_rendercmd_draw(cmd, 6, 1);
		box_rendercmd_end_renderstage(cmd);

		box_engine_render_frame(engine, cmd);
	}

	box_destroy_engine(engine);
	return 0;
}