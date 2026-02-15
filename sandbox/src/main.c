#include "engine.h"
#include "renderer/renderer_backend.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

VkResult create_vulkan_surface(box_platform* plat_state, VkInstance instance, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface) {
	GLFWwindow* window = (GLFWwindow*)plat_state->internal_state;
	return glfwCreateWindowSurface(instance, window, allocator, surface);
}

u32 get_vulkan_extensions(box_platform* plat_state, const char*** out_array) {
	uint32_t count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    *out_array = extensions;
    return count;
}

int main(int argc, char** argv) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow* window = glfwCreateWindow(640, 480, "Hello, World", NULL, NULL);

    box_platform platform = {0};
    platform.internal_state = window;
    platform.get_required_vulkan_extensions = get_vulkan_extensions;
    platform.create_vulkan_surface = create_vulkan_surface;

    box_renderer_backend_config config = box_renderer_backend_default_config();
	config.modes = RENDERER_MODE_GRAPHICS | RENDERER_MODE_TRANSFER;
	config.sampler_anisotropy = TRUE;

    box_renderer_backend backend = {};
    if (!box_renderer_backend_create(&config, (uvec2) { 640, 480 }, "Hello, World", &platform, &backend)) {
        printf("Failed to create renderer backend\n");
        return 1;
    }

	f32 vertices[] = {
		-0.5f, -0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f,
		 0.5f, -0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f,
		 0.5f,  0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f,
		-0.5f,  0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f
	};

	u16 indices[] = { 0, 1, 2, 2, 3, 0 };

    const char* shaders[] = { "assets/shader_base.vert.spv", "assets/shader_base.frag.spv" };

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

    box_renderstage renderstage =  box_renderstage_graphics_default(shaders, BX_ARRAYSIZE(shaders));
	renderstage.graphics.vertex_buffer = &vert_buffer;
	renderstage.graphics.index_buffer = &index_buffer;
	renderstage.graphics.topology_type = BOX_VERTEX_TOPOLOGY_TRIANGLES;

    renderstage.vertex_attributes[0] = (box_render_format) { .type = BOX_FORMAT_TYPE_FLOAT32, .channel_count = 2 }; // Position (X, Y)
    renderstage.vertex_attributes[1] = (box_render_format) { .type = BOX_FORMAT_TYPE_FLOAT32, .channel_count = 3 }; // Colour   (R, G, B)
    renderstage.vertex_attributes[2] = (box_render_format) { .type = BOX_FORMAT_TYPE_FLOAT32, .channel_count = 2 }; // Texcoord (tX, tY)
    renderstage.vertex_attribute_count = 3;

    renderstage.descriptors[0] = (box_descriptor_desc) {
        // Texture Sampler
        .binding = 0,
        .descriptor_type = BOX_DESCRIPTOR_TYPE_IMAGE_SAMPLER,
        .stage_type = BOX_SHADER_STAGE_TYPE_FRAGMENT,
    };
    renderstage.descriptor_count = 1;

    if (!backend.create_renderstage(&backend, &renderstage)) {
        printf("Failed to create renderstage\n");
        return 1;
    }

	int width, height, channels;

	stbi_set_flip_vertically_on_load(1);
	stbi_uc* data = stbi_load("assets/eyeball.png", &width, &height, &channels, 0);

	box_texture texture = box_texture_default();
	texture.max_anisotropy = backend.capabilities.max_anisotropy;
	texture.image_format = (box_render_format) { .type = BOX_FORMAT_TYPE_SRGB, .channel_count = channels, .normalized = TRUE };
	texture.size = (uvec2) { width, height };

	if (!backend.create_texture(&backend, &texture) ||
		!backend.upload_to_texture(&backend, &texture, data, (uvec2) { 0, 0 }, texture.size)) {
        printf("Failed to create texture\n");
        return 1;
	}

	box_update_descriptors descriptors[] = {
    	{
			.binding = 0,
			.type = BOX_DESCRIPTOR_TYPE_IMAGE_SAMPLER,
			.texture = &texture,
		}
	};

	if (!backend.update_renderstage_descriptors(&backend, &renderstage, descriptors, BX_ARRAYSIZE(descriptors))) {
        printf("Failed to update renderstage descriptors\n");
        return 1;
	}

	box_rendercmd rendercmd = {};
	box_rendercmd_context submit_context = {};

    f64 last_time = glfwGetTime() * 1000.0;
	while (!glfwWindowShouldClose(window)) {
        f64 now = glfwGetTime() * 1000.0;
        f64 delta_time = now - last_time;
        last_time = now;

		if (backend.begin_frame(&backend, delta_time)) {

			// ------------------
			box_rendercmd_begin(&rendercmd);
			box_rendercmd_set_clear_colour(&rendercmd, 0.1f, 0.1f, 0.1f);

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

		glfwPollEvents();
	}

	backend.wait_until_idle(&backend, 0);

    box_rendercmd_destroy(&rendercmd);

	backend.destroy_texture(&backend, &texture);
	backend.destroy_renderbuffer(&backend, &index_buffer);
	backend.destroy_renderbuffer(&backend, &vert_buffer);
    backend.destroy_renderstage(&backend, &renderstage);
    box_renderer_backend_destroy(&backend);

    glfwTerminate();
    return 0;
}