#include "engine.h"

#include "renderer/renderer_cmd.h"
#include "renderer/render_layout.h"
#include "renderer/renderer_types.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

box_texture* load_texture(box_engine* engine, const char* filepath) {
	int width, height, channels;

	stbi_set_flip_vertically_on_load(TRUE);
	stbi_uc* data = stbi_load(filepath, &width, &height, &channels, 0);

	box_render_format tex_format = {
		.type = BOX_FORMAT_TYPE_SRGB,
		.channel_count = channels,
		.normalized = TRUE,
	};
	return box_engine_create_texture(engine, (uvec2) { width, height }, tex_format, BOX_FILTER_TYPE_NEAREST, BOX_ADDRESS_MODE_REPEAT, data);
}

int main(int argc, char** argv) {
	// Sets default config for box_engine.
	box_config app_config = box_default_config();
	app_config.render_config.modes = RENDERER_MODE_GRAPHICS | RENDERER_MODE_TRANSFER;
	app_config.render_config.sampler_anisotropy = TRUE;

	box_engine* engine = box_create_engine(&app_config);
	if (engine == NULL)
		BX_FATAL("Failed to init Boxel engine!");

	f32 vertices[] = {
		-0.5f, -0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f,
		 0.5f, -0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f,
		 0.5f,  0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f,
		-0.5f,  0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f
	};

	u16 indices[] = { 0, 1, 2, 2, 3, 0 };

	box_render_layout layout = { 0 };

	// --- General layout settings -- //
	box_render_layout_set_topology(&layout, BOX_VERTEX_TOPOLOGY_TRIANGLES); // Triangle list
	box_render_layout_set_index_type(&layout, BOX_FORMAT_TYPE_UINT16);      // Index type

	// --- Layout of vertex attributs --- //
	box_render_layout_add(&layout, (box_render_format) { .type = BOX_FORMAT_TYPE_FLOAT32, .channel_count = 2 }); // Position
	box_render_layout_add(&layout, (box_render_format) { .type = BOX_FORMAT_TYPE_FLOAT32, .channel_count = 3 }); // Colour
	box_render_layout_add(&layout, (box_render_format) { .type = BOX_FORMAT_TYPE_FLOAT32, .channel_count = 2 }); // Texcoord

	// --- Render layout descriptors & uniforms --- //
	box_render_layout_add_descriptor(&layout, BOX_DESCRIPTOR_TYPE_IMAGE_SAMPLER, BOX_SHADER_STAGE_TYPE_FRAGMENT);
	box_render_layout_end(&layout);

	const char* graphics_shaders[] = { "assets/shader_base.vert.spv", "assets/shader_base.frag.spv" };

	box_renderbuffer* vert_buffer = box_engine_create_renderbuffer(engine, BOX_RENDERBUFFER_USAGE_VERTEX, sizeof(vertices), vertices);
	box_renderbuffer* index_buffer = box_engine_create_renderbuffer(engine, BOX_RENDERBUFFER_USAGE_INDEX, sizeof(indices), indices);

	box_renderstage* renderstage = box_engine_create_renderstage(engine, &layout, BX_ARRAYSIZE(graphics_shaders), graphics_shaders, vert_buffer, index_buffer);

	box_texture* eye_texture = load_texture(engine, "assets/eyeball.png");

	box_engine_prepare_resources(engine);

	while (box_engine_is_running(engine)) {
		// Checks render thread is minimised or paused.
		if (box_engine_should_skip_frame(engine))
			continue;

		box_rendercmd* cmd = box_engine_next_frame(engine);
		{
			box_rendercmd_set_clear_colour(cmd, 0.1f, 0.1f, 0.1f);

			box_rendercmd_begin_renderstage(cmd, renderstage);
			box_rendercmd_set_texture(cmd, eye_texture, 0);

			box_rendercmd_draw_indexed(cmd, 6, 1);
			box_rendercmd_end_renderstage(cmd);
		}
	}

	box_destroy_engine(engine);
	return 0;
}