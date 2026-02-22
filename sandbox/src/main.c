#include "platform/platform.h"
#include "renderer/renderer_backend.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

int main(int argc, char** argv) {
	box_window_config window_config = box_window_default_config();
	window_config.window_size = (uvec2) { 640, 640 };
	window_config.title = "Test Window";

	box_platform platform = {};
	if (!platform_start(&platform, &window_config)) {
        printf("Failed to open window / platform surface\n");
        return 1;
	}

    box_renderer_backend_config config = box_renderer_backend_default_config();
	config.modes = RENDERER_MODE_GRAPHICS | RENDERER_MODE_COMPUTE | RENDERER_MODE_TRANSFER;
	config.sampler_anisotropy = TRUE;

    box_renderer_backend backend = {};
    if (!box_renderer_backend_create(&config, window_config.window_size, window_config.title, &platform, &backend)) {
        printf("Failed to create renderer backend\n");
        return 1;
    }

	f32 vertices[] = {
	//    x,     y,    r,    g,    b,    u,    v
		-1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, // Bottom-Left
		 1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, // Bottom-Right
		 1.0f,  1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, // Top-Right
		-1.0f,  1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f  // Top-Left
	};

	u16 indices[] = { 0, 1, 2, 2, 3, 0 };

    const char* gfx_shaders[] = { "assets/shader_base.vert.spv", "assets/shader_base.frag.spv" };
    const char* comp_shaders = "assets/test_compute.comp.spv";

	box_texture storage_image = box_texture_default();
	storage_image.image_format = (box_render_format) { .type = BOX_FORMAT_TYPE_FLOAT32, .channel_count = 4, .normalized = TRUE };
	storage_image.usage = BOX_TEXTURE_USAGE_STORAGE | BOX_TEXTURE_USAGE_SAMPLED;
	storage_image.size = window_config.window_size;
	if (!backend.create_texture(&backend, &storage_image)) {
		printf("Failed to create storage image\n");
		return 1;
	}

	box_renderstage compstage = box_renderstage_compute_default(&comp_shaders, 1);
	compstage.descriptors[0] = (box_descriptor_desc) {
		// Storage image
		.binding = 0,
		.descriptor_type = BOX_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		.stage_type = BOX_SHADER_STAGE_TYPE_COMPUTE,
	};
	compstage.descriptor_count = 1;
	if (!backend.create_renderstage(&backend, &backend.main_rendertarget, &compstage)) {
        printf("Failed to create compute stage\n");
        return 1;
	}

	box_renderbuffer vert_buffer = box_renderbuffer_default();
	vert_buffer.usage = BOX_RENDERBUFFER_USAGE_VERTEX;
	vert_buffer.buffer_size = sizeof(vertices);

	if (!backend.create_renderbuffer(&backend, &vert_buffer) ||
		!backend.upload_to_renderbuffer(&backend, &vert_buffer, vertices, 0, vert_buffer.buffer_size)) {
        printf("Failed to create renderbuffer\n");
        return 1;
	}

	box_renderbuffer index_buffer = box_renderbuffer_default();
	index_buffer.usage = BOX_RENDERBUFFER_USAGE_INDEX;
	index_buffer.buffer_size = sizeof(indices);

	if (!backend.create_renderbuffer(&backend, &index_buffer) ||
		!backend.upload_to_renderbuffer(&backend, &index_buffer, indices, 0, index_buffer.buffer_size)) {
        printf("Failed to create renderbuffer\n");
        return 1;
	}

	box_renderstage renderstage = box_renderstage_graphics_default(gfx_shaders, BX_ARRAYSIZE(gfx_shaders));
    renderstage.graphics.vertex_attributes[0] = (box_render_format) { .type = BOX_FORMAT_TYPE_FLOAT32, .channel_count = 2 }; // Position (X, Y)
    renderstage.graphics.vertex_attributes[1] = (box_render_format) { .type = BOX_FORMAT_TYPE_FLOAT32, .channel_count = 3 }; // Colour   (R, G, B)
    renderstage.graphics.vertex_attributes[2] = (box_render_format) { .type = BOX_FORMAT_TYPE_FLOAT32, .channel_count = 2 }; // Texcoord (tX, tY)
    renderstage.graphics.vertex_attribute_count = 3;

	renderstage.graphics.vertex_buffer = &vert_buffer;
	renderstage.graphics.index_buffer = &index_buffer;

	renderstage.descriptors[0] = (box_descriptor_desc) {
		// Sampled image
		.binding = 0,
		.descriptor_type = BOX_DESCRIPTOR_TYPE_IMAGE_SAMPLER,
		.stage_type = BOX_SHADER_STAGE_TYPE_FRAGMENT,
	};
	renderstage.descriptor_count = 1;
	if (!backend.create_renderstage(&backend, &backend.main_rendertarget, &renderstage)) {
        printf("Failed to create compute stage\n");
        return 1;
	}

	box_update_descriptors comp_descriptors[] = {
		{
			.binding = 0,
			.type = BOX_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.texture = &storage_image,
		},
	};
	if (!backend.update_renderstage_descriptors(&backend, &compstage, comp_descriptors, BX_ARRAYSIZE(comp_descriptors))) {
        printf("Failed to update renderstage descriptors\n");
        return 1;
	}

	box_update_descriptors gfx_descriptors[] = {
		{
			.binding = 0,
			.type = BOX_DESCRIPTOR_TYPE_IMAGE_SAMPLER,
			.texture = &storage_image,
		},
	};
	if (!backend.update_renderstage_descriptors(&backend, &renderstage, gfx_descriptors, BX_ARRAYSIZE(gfx_descriptors))) {
        printf("Failed to update renderstage descriptors\n");
        return 1;
	}

	box_rendercmd rendercmd = {};
	box_rendercmd_context submit_context = {};

    f64 last_time = platform_get_absolute_time();
	while (!platform_should_close_window(&platform)) {
        f64 now = platform_get_absolute_time();
        f64 delta_time = now - last_time;
        last_time = now;

		if (backend.begin_frame(&backend, delta_time)) {

			// ------------------
			box_rendercmd_begin(&rendercmd);
			box_rendercmd_begin_renderstage(&rendercmd, &compstage);
			box_rendercmd_dispatch(&rendercmd, 
				(window_config.window_size.x + 7) / 8,
				(window_config.window_size.y + 7) / 8,
				1);
			box_rendercmd_end_renderstage(&rendercmd);
			
			// Says the current submission and the submission  renderstage will be on will wait on it, so
			// box_rendercmd_begin_renderstage will also transfer ownership 
			box_rendercmd_memory_barrier(&rendercmd, 
				&compstage,                    &renderstage,
				BOX_ACCESS_FLAGS_SHADER_WRITE, BOX_ACCESS_FLAGS_SHADER_READ);
			
			// Transfer ownership of descriptors...
			// vkCmdPipelineBarrier(send to renderstage) 
			// Signal / wait on queues
			// vkCmdPipelineBarrier(receive from compstage) 
			
			box_rendercmd_bind_rendertarget(&rendercmd, &backend.main_rendertarget);

			box_rendercmd_begin_renderstage(&rendercmd, &renderstage);
			box_rendercmd_draw_indexed(&rendercmd, 6, 1);
			box_rendercmd_end_renderstage(&rendercmd);
			box_rendercmd_end(&rendercmd);
			// ------------------

			bzero_memory(&submit_context, sizeof(box_rendercmd_context));
			if (!box_renderer_backend_submit_rendercmd(&backend, &submit_context, &rendercmd)) {
				printf("Failed to create submit render command\n");
				return 1;
			}

			if (!backend.end_frame(&backend)) {
				printf("Failed to create end frame\n");
				return 1;
			}
		}

		platform_pump_messages(&platform);
	}

    box_rendercmd_destroy(&rendercmd);

	backend.wait_until_idle(&backend, UINT64_MAX);
	backend.destroy_renderbuffer(&backend, &vert_buffer);
	backend.destroy_renderbuffer(&backend, &index_buffer);
	backend.destroy_renderstage(&backend, &renderstage);
	backend.destroy_texture(&backend, &storage_image);
	backend.destroy_renderstage(&backend, &compstage);
    box_renderer_backend_destroy(&backend);

	platform_shutdown(&platform);
	memory_shutdown();
    return 0;
}