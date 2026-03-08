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
        printf("Failed to open window\n");
        goto failed_init;
	}

	box_renderer_backend_config render_config = box_renderer_backend_default_config();
	
	box_renderer_backend backend = {};
	if (!box_renderer_backend_create(&render_config, window_config.window_size, window_config.title, &platform, &backend)) {
        printf("Failed to create renderer backend\n");
        goto failed_init;
	}

	box_rendertarget* main_rendertarget = NULL;
	if (!backend.create_rendertarget_on_platform(&backend, &main_rendertarget)) {
        printf("Failed to create window rendertarget\n");
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

		if (backend.begin_frame(&backend, delta_time)) {

			// ------------------
			box_rendercmd_begin(&rendercmd);
			box_rendercmd_bind_rendertarget(&rendercmd, main_rendertarget);
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

failed_init:
	backend.wait_until_idle(&backend, UINT64_MAX);
    box_renderer_backend_destroy(&backend);
	platform_shutdown(&platform);

	memory_shutdown();
	return 0;
}