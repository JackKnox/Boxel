#include "defines.h"
#include "renderer_backend.h"

#include "utils/darray.h"

#include "vulkan/vulkan_backend.h"

box_renderer_backend_config renderer_backend_default_config() {
    box_renderer_backend_config configuration = {0}; // fill with zeros
    configuration.modes = RENDERER_MODE_GRAPHICS /* | RENDERER_MODE_COMPUTE */;
	configuration.frames_in_flight = 3;

#if BOX_ENABLE_VALIDATION
    configuration.enable_validation = TRUE;
#else
    configuration.enable_validation = FALSE;
#endif
    return configuration;
}

b8 renderer_backend_create(box_renderer_backend_config* config, uvec2 starting_size, const char* application_name, struct box_platform* plat_state, box_renderer_backend* out_renderer_backend) {
    BX_ASSERT(plat_state != NULL && config != NULL && out_renderer_backend != NULL && "Invalid arguments passed to renderer_backend_create");
    out_renderer_backend->plat_state = plat_state;

    if (config->api_type == RENDERER_BACKEND_TYPE_VULKAN) {
        out_renderer_backend->initialize = vulkan_renderer_backend_initialize;
        out_renderer_backend->shutdown = vulkan_renderer_backend_shutdown;
        out_renderer_backend->wait_until_idle = vulkan_renderer_backend_wait_until_idle;
        out_renderer_backend->resized = vulkan_renderer_backend_on_resized;
        out_renderer_backend->begin_frame = vulkan_renderer_backend_begin_frame;
        out_renderer_backend->playback_rendercmd = vulkan_renderer_playback_rendercmd;
        out_renderer_backend->end_frame = vulkan_renderer_backend_end_frame;
        out_renderer_backend->create_internal_renderstage = vulkan_renderer_create_renderstage;
        out_renderer_backend->destroy_internal_renderstage = vulkan_renderer_destroy_renderstage;
        out_renderer_backend->create_internal_renderbuffer = vulkan_renderer_create_renderbuffer;
        out_renderer_backend->upload_to_renderbuffer = vulkan_renderer_upload_to_renderbuffer;
        out_renderer_backend->destroy_internal_renderbuffer = vulkan_renderer_destroy_renderbuffer;
        out_renderer_backend->create_internal_texture = vulkan_renderer_create_texture;
        out_renderer_backend->destroy_internal_texture = vulkan_renderer_destroy_texture;
    }
    else {
        BX_ERROR("Unsupported renderer backend type (%i).", config->api_type);
        return FALSE;
    }

    out_renderer_backend->initialize(out_renderer_backend, config, starting_size, application_name);
    return TRUE;
}

void renderer_backend_destroy(box_renderer_backend* renderer_backend) {
    if (renderer_backend->shutdown != NULL) 
        renderer_backend->shutdown(renderer_backend);
    
    bzero_memory(renderer_backend, sizeof(*renderer_backend));
}