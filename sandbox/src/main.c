#include "platform/platform.h"
#include "renderer/renderer_backend.h"

#include "platform/filesystem.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

box_shader_src read_file(const char* p) {
	u64 size = 0;
	const void* f = filesystem_read_entire_binary_file(p, &size);
	return (box_shader_src) { .data = f, .size = size };
}

int main(int argc, char** argv) {
	box_window_config window_config = box_window_default_config();
	window_config.window_size = (uvec2) { 640, 640 };
	window_config.title = "Test Window";

	box_platform platform = {};
	if (!platform_start(&platform, &window_config)) {
        printf("Failed to open window\n");
        goto failed_init;
	}

	box_renderer_backend_config render_config = box_renderer_backend_default_config();
	render_config.modes = RENDERER_MODE_GRAPHICS | RENDERER_MODE_COMPUTE | RENDERER_MODE_TRANSFER;
	render_config.application_name = window_config.title;
	render_config.starting_size = window_config.window_size;
	render_config.sampler_anisotropy = TRUE;
	
	box_renderer_backend backend = {};
	if (!box_renderer_backend_create(&backend, &render_config, &platform)) {
        printf("Failed to create renderer backend\n");
        goto failed_init;
	}

	f32 vertices[] = {
	//    x,     y,    r,    g,    b,    u,    v
		-1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, // Bottom-Left
		 1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, // Bottom-Right
		 1.0f,  1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, // Top-Right
		-1.0f,  1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f  // Top-Left
	};

	u16 indices[] = { 0, 1, 2, 2, 3, 0 };

	box_render_format vert_attributes[] = {
	//     Position (X, Y)         Colour (R, G, B)      Texcoord (tX, tY)
		BOX_FORMAT_RG32_FLOAT, BOX_FORMAT_RGB32_FLOAT, BOX_FORMAT_RG32_FLOAT
	};

	box_descriptor_desc vert_descriptors[] = {
	//     Sampled image
		{ .binding = 0, .descriptor_type = BOX_DESCRIPTOR_TYPE_IMAGE_SAMPLER, .stage_type = BOX_SHADER_STAGE_TYPE_FRAGMENT },
	};

	int width, height, channels;
	stbi_set_flip_vertically_on_load(TRUE);
	stbi_uc* data = stbi_load("assets/eyeball.png", &width, &height, &channels, 0);

	box_texture_config tex_config = box_texture_default();
	tex_config.size = (uvec2) { width, height };
	tex_config.usage = BOX_TEXTURE_USAGE_SAMPLED;
	tex_config.image_format = BOX_RENDER_FORMAT(BOX_FORMAT_UINT, 8, channels, BOX_FORMAT_FLAG_SRGB);

	box_texture texture = {};
	if (!backend.create_texture(&backend, &tex_config, &texture) ||
		!backend.upload_to_texture(&backend, &texture, data, (uvec2) { 0,0 }, tex_config.size)) {
		printf("Failed to create sampled image\n");
        goto failed_init;
	}

	box_renderbuffer_config vert_buffer_config = box_renderbuffer_default();
	vert_buffer_config.buffer_size = sizeof(vertices);
	vert_buffer_config.usage = BOX_RENDERBUFFER_USAGE_VERTEX;

	box_renderbuffer vertex_buffer = {};
	if (!backend.create_renderbuffer(&backend, &vert_buffer_config, &vertex_buffer) ||
		!backend.upload_to_renderbuffer(&backend, &vertex_buffer, vertices, 0, sizeof(vertices))) {
		printf("Failed to create renderbuffer (vertex)\n");
		goto failed_init;
	}

	box_renderbuffer_config indx_buffer_config = box_renderbuffer_default();
	indx_buffer_config.buffer_size = sizeof(indices);
	indx_buffer_config.usage = BOX_RENDERBUFFER_USAGE_INDEX;

	box_renderbuffer index_buffer = {};
	if (!backend.create_renderbuffer(&backend, &indx_buffer_config, &index_buffer) ||
		!backend.upload_to_renderbuffer(&backend, &index_buffer, indices, 0, sizeof(indices))) {
		printf("Failed to create renderbuffer (index)\n");
		goto failed_init;
	}

	box_graphicstage_config graphicstage_config = box_renderstage_default_graphic();
	graphicstage_config.layout.stages[BOX_SHADER_STAGE_TYPE_VERTEX] = read_file("assets/shader_base.vert.spv");
	graphicstage_config.layout.stages[BOX_SHADER_STAGE_TYPE_FRAGMENT] = read_file("assets/shader_base.frag.spv");
	graphicstage_config.layout.descriptors = vert_descriptors;
	graphicstage_config.layout.descriptor_count = BX_ARRAYSIZE(vert_descriptors);

	graphicstage_config.vertex_attributes = vert_attributes;
	graphicstage_config.vertex_attribute_count = BX_ARRAYSIZE(vert_attributes);
	graphicstage_config.vertex_buffer = &vertex_buffer;
	graphicstage_config.index_buffer = &index_buffer;

	box_renderstage renderstage = {};
	if (!backend.create_graphicstage(&backend, &graphicstage_config, &backend.main_rendertarget, &renderstage)) {
		printf("Failed to create graphics renderpass\n");
		goto failed_init;
	}

	box_update_descriptors update_descriptors[] = {
		{
			.renderstage = &renderstage,

			.binding = 0,
			.type = BOX_DESCRIPTOR_TYPE_IMAGE_SAMPLER,
			.texture = &texture,
		}
	};

	if (!backend.update_renderstage_descriptors(&backend, update_descriptors, BX_ARRAYSIZE(update_descriptors))) {
		printf("Failed to update renderstage descriptors\n");
		goto failed_init;
	}

	show_memory_stats();

	box_rendercmd rendercmd = {};
	box_rendercmd_context submit_context = {};

	f64 last_time = platform_get_absolute_time();
	while (!platform_should_close_window(&platform)) {
        f64 now = platform_get_absolute_time();
        f64 delta_time = now - last_time;
        last_time = now;
		
		backend.main_rendertarget.clear_colour = 0xFF0000FF;
		if (backend.begin_frame(&backend, delta_time)) {

			// ------------------
			box_rendercmd_begin(&rendercmd);
			box_rendercmd_bind_rendertarget(&rendercmd, &backend.main_rendertarget);
			box_rendercmd_begin_renderstage(&rendercmd, &renderstage);
			box_rendercmd_draw_indexed(&rendercmd, 6, 1);
			box_rendercmd_end_renderstage(&rendercmd);
			box_rendercmd_end(&rendercmd);
			
			// ------------------
			bzero_memory(&submit_context, sizeof(box_rendercmd_context));
			if (!box_renderer_backend_submit_rendercmd(&backend, &submit_context, &rendercmd)) {
				printf("Failed to create submit render command\n");
				goto failed_init;
			}

			if (!backend.end_frame(&backend)) {
				printf("Failed to create end frame\n");
				goto failed_init;
			}
		}

		platform_pump_messages(&platform);
	}

	box_rendercmd_destroy(&rendercmd);

failed_init:
	backend.destroy_renderstage(&backend, &renderstage);
	backend.destroy_texture(&backend, &texture);
	backend.destroy_renderbuffer(&backend, &vertex_buffer);
	backend.destroy_renderbuffer(&backend, &index_buffer);
    box_renderer_backend_destroy(&backend);
	platform_shutdown(&platform);

	memory_shutdown();
	return 0;
}