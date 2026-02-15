#include "defines.h"
#include "renderer_backend.h"

#include "platform/platform.h"

#include "vulkan/vulkan_backend.h"

u64 box_render_format_size(box_render_format format) {
	u64 base_size = 1;
	
	switch (format.type) {
	case BOX_FORMAT_TYPE_SINT8:
	case BOX_FORMAT_TYPE_UINT8:
	case BOX_FORMAT_TYPE_BOOL:
		base_size = 1;
		break;

	case BOX_FORMAT_TYPE_SINT16:
	case BOX_FORMAT_TYPE_UINT16:
		base_size = 2;
		break;

	case BOX_FORMAT_TYPE_SINT32:
	case BOX_FORMAT_TYPE_UINT32:
	case BOX_FORMAT_TYPE_FLOAT32:
		base_size = 4;
		break;
    }

	return base_size * format.channel_count;
}

box_renderer_backend_config box_renderer_backend_default_config() {
    box_renderer_backend_config configuration = {0}; // fill with zeros
    configuration.modes = RENDERER_MODE_GRAPHICS;
	configuration.frames_in_flight = 3;

#if BOX_ENABLE_VALIDATION
    configuration.enable_validation = TRUE;
#else
    configuration.enable_validation = FALSE;
#endif
    return configuration;
}

b8 box_renderer_backend_create(box_renderer_backend_config* config, uvec2 starting_size, const char* application_name, struct box_platform* plat_state, box_renderer_backend* out_renderer_backend) {
    BX_ASSERT(starting_size.x != 0 && starting_size.y != 0 && application_name != NULL && plat_state != NULL && config != NULL && out_renderer_backend != NULL && "Invalid arguments passed to box_renderer_backend_create");
    BX_ASSERT(plat_state->create_vulkan_surface != NULL && plat_state->get_required_vulkan_extensions != NULL && "Invalid box_platform passed to box_renderer_backend_create");
    
	if (config->modes == 0 || config->frames_in_flight <= 0) {
		BX_ERROR("Invalid box_platform passed to box_renderer_backend_create");
		return FALSE;
	}
	
	out_renderer_backend->plat_state = plat_state;

    if (config->api_type == RENDERER_BACKEND_TYPE_VULKAN) {
        out_renderer_backend->initialize = vulkan_renderer_backend_initialize;
        out_renderer_backend->shutdown = vulkan_renderer_backend_shutdown;
        out_renderer_backend->wait_until_idle = vulkan_renderer_backend_wait_until_idle;
        out_renderer_backend->resized = vulkan_renderer_backend_on_resized;
        out_renderer_backend->begin_frame = vulkan_renderer_backend_begin_frame;
        out_renderer_backend->execute_command = vulkan_renderer_execute_command;
        out_renderer_backend->end_frame = vulkan_renderer_backend_end_frame;
        out_renderer_backend->create_renderstage = vulkan_renderer_create_renderstage;
		out_renderer_backend->update_renderstage_descriptors = vulkan_renderer_update_renderstage_descriptors;
        out_renderer_backend->destroy_renderstage = vulkan_renderer_destroy_renderstage;
        out_renderer_backend->create_renderbuffer = vulkan_renderer_create_renderbuffer;
        out_renderer_backend->upload_to_renderbuffer = vulkan_renderer_upload_to_renderbuffer;
        out_renderer_backend->destroy_renderbuffer = vulkan_renderer_destroy_renderbuffer;
        out_renderer_backend->create_texture = vulkan_renderer_create_texture;
		out_renderer_backend->upload_to_texture = vulkan_renderer_upload_to_texure;
        out_renderer_backend->destroy_texture = vulkan_renderer_destroy_texture;
    }
    else {
        BX_ERROR("Unsupported renderer backend type (%i).", config->api_type);
        return FALSE;
    }

    out_renderer_backend->initialize(out_renderer_backend, config, starting_size, application_name);
    return TRUE;
}

void box_renderer_backend_destroy(box_renderer_backend* renderer_backend) {
    if (renderer_backend->shutdown != NULL) 
        renderer_backend->shutdown(renderer_backend);
    
    bzero_memory(renderer_backend, sizeof(box_renderer_backend));
}

b8 box_renderer_backend_submit_rendercmd(box_renderer_backend* renderer_backend, box_rendercmd_context* playback_context, box_rendercmd* rendercmd) {
	u8* cursor = 0;
	while (freelist_next_block(&rendercmd->buffer, &cursor)) {
		rendercmd_header* hdr = (rendercmd_header*)cursor;
		rendercmd_payload* payload = (rendercmd_payload*)(cursor + sizeof(rendercmd_header));

		if (hdr->supported_mode)
			playback_context->current_mode = hdr->supported_mode;

		switch (hdr->type) {
		case RENDERCMD_BEGIN_RENDERSTAGE:
#if BOX_ENABLE_VALIDATION
			if (playback_context->current_shader != NULL) {
				BX_ERROR("Tried to begin renderstage twice in box_rendercmd.");
				return FALSE;
			}
#endif

			playback_context->current_shader = payload->begin_renderstage.renderstage;
			break;

		case RENDERCMD_END_RENDERSTAGE:
#if BOX_ENABLE_VALIDATION
			if (playback_context->current_shader == NULL) {
				BX_ERROR("Tried to end renderstage twice in box_rendercmd.");
				return FALSE;
			}
#endif

			break;

		case RENDERCMD_DRAW:
		case RENDERCMD_DRAW_INDEXED:
		case RENDERCMD_DISPATCH:
#if BOX_ENABLE_VALIDATION
			if (playback_context->current_shader == NULL) {
				BX_ERROR("Tried to dispatch draw call without a renderstage in box_rendercmd.");
				return FALSE;
			}
#endif
			break;
		}

		renderer_backend->execute_command(renderer_backend, playback_context, hdr, payload);
	}

	return TRUE;
}
