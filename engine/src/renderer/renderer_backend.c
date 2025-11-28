#include "defines.h"
#include "renderer_backend.h"

#include "utils/darray.h"

#include "vulkan/vulkan_backend.h"

renderer_backend_config renderer_backend_default_config() {
    renderer_backend_config configuration = {0}; // fill with zeros
	configuration.swapchain_frame_count = 3;
	configuration.enable_validation = TRUE;
	configuration.graphics_pipeline = TRUE;
	configuration.required_extensions = darray_create(const char*);
    return configuration;
}

b8 renderer_backend_create(renderer_backend_type type, struct box_platform* plat_state, renderer_backend* out_renderer_backend) {
    if (!out_renderer_backend) return FALSE;
    out_renderer_backend->plat_state = plat_state;

    if (type == RENDERER_BACKEND_TYPE_VULKAN) {
        out_renderer_backend->initialize = vulkan_renderer_backend_initialize;
        out_renderer_backend->shutdown = vulkan_renderer_backend_shutdown;
        out_renderer_backend->resized = vulkan_renderer_backend_on_resized;
        out_renderer_backend->begin_frame = vulkan_renderer_backend_begin_frame;
        out_renderer_backend->playback_rendercmd = vulkan_renderer_playback_rendercmd;
        out_renderer_backend->end_frame = vulkan_renderer_backend_end_frame;

        return TRUE;
    }

    BX_ERROR("Unsupported renderer backend type (%i).", type);
    return FALSE;
}

void renderer_backend_destroy(renderer_backend* renderer_backend) {
    platform_zero_memory(renderer_backend, sizeof(renderer_backend));
}