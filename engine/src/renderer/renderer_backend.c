#include "defines.h"
#include "renderer_backend.h"

#include "vulkan/vulkan_backend.h"
#include "vulkan/vulkan_renderbuffer.h"
#include "vulkan/vulkan_renderstage.h"
#include "vulkan/vulkan_rendertarget.h"
#include "vulkan/vulkan_texture.h"
#include "renderer_types.h"

u64 box_render_format_size(box_render_format format) {
    u32 bits_index = (format >> 16) & 0xF;
    u32 channels   = (format >> 12) & 0xF;

	return bits_index * channels + channels; // ((b + 1) * 8c) / 8
}

b8 box_render_format_normalized(box_render_format format) {
    u32 flags = (format >> 24) & 0xFF;
    return (flags & BOX_RENDER_FORMAT_FLAG_NORMALIZED) != 0;
}

u32 box_render_format_channel_count(box_render_format format) {
    return (format >> 12) & 0xF;
}
box_renderer_backend_config box_renderer_backend_default_config() {
    box_renderer_backend_config configuration = {};
    configuration.modes = RENDERER_MODE_GRAPHICS;
	configuration.frames_in_flight = 3;

#if BOX_ENABLE_VALIDATION
    configuration.enable_validation = TRUE;
#endif
    return configuration;
}

b8 box_renderer_backend_create(box_renderer_backend* renderer_backend, box_renderer_backend_config* config, box_platform* platform) {
	BX_ASSERT(renderer_backend != NULL && config != NULL && config->application_name != NULL && (platform != NULL || config->main_attachments != NULL) && "Invalid arguments passed to box_renderer_backend_create");

#if BOX_ENABLE_VALIDATION
	if (platform->internal_renderer_state != NULL) {
		BX_ERROR("box_renderer_backend_create(): Cannot attach more than 1 renderer backend to the same platform");
		return FALSE;
	}

	if (config->frames_in_flight < 1) {
		BX_ERROR("box_renderer_backend_create(): Must create renderer backend with at least 1 frame(s) in flight: %u", config->frames_in_flight);
		return FALSE;
	}

	if (!platform && config->main_attachment_count == 0) {
		BX_ERROR("box_renderer_backend_create(): Cannot set zero attachments without platform state attached to renderer");
		return FALSE;
	}
#endif

	renderer_backend->platform = platform;

    if (config->api_type == RENDERER_BACKEND_TYPE_VULKAN) {
        renderer_backend->initialize      = vulkan_renderer_backend_initialize;
        renderer_backend->shutdown        = vulkan_renderer_backend_shutdown;
        renderer_backend->resized         = vulkan_renderer_backend_on_resized;

        renderer_backend->begin_frame     = vulkan_renderer_backend_begin_frame;
        renderer_backend->execute_command = vulkan_renderer_execute_command;
        renderer_backend->end_frame       = vulkan_renderer_backend_end_frame;

        renderer_backend->create_graphicstage            = vulkan_renderstage_create_graphic;
		renderer_backend->create_computestage            = vulkan_renderstage_create_compute;
		renderer_backend->update_renderstage_descriptors = vulkan_renderstage_update_descriptors;
        renderer_backend->destroy_renderstage            = vulkan_renderstage_destroy;

        renderer_backend->create_renderbuffer            = vulkan_renderbuffer_create;
        renderer_backend->upload_to_renderbuffer         = vulkan_renderbuffer_upload_data;
        renderer_backend->destroy_renderbuffer           = vulkan_renderbuffer_destroy;

        renderer_backend->create_texture       = vulkan_texture_create;
		renderer_backend->upload_to_texture    = vulkan_texture_upload_data;
        renderer_backend->destroy_texture      = vulkan_texture_destroy;

   	 	renderer_backend->create_rendertarget  = vulkan_rendertarget_create;
    	renderer_backend->destroy_rendertarget = vulkan_rendertarget_destroy;
    }
    else {
        BX_ERROR("Unsupported renderer backend type (%i).", config->api_type);
        return FALSE;
    }
	
    return renderer_backend->initialize(renderer_backend, config);
}

void box_renderer_backend_destroy(box_renderer_backend* renderer_backend) {
	BX_ASSERT(renderer_backend != NULL && "Invalid arguments passed to box_renderer_backend_destroy");
    if (renderer_backend->shutdown != NULL) 
        renderer_backend->shutdown(renderer_backend);
    
    bzero_memory(renderer_backend, sizeof(box_renderer_backend));
}

b8 box_renderer_backend_submit_rendercmd(box_renderer_backend* renderer_backend, box_rendercmd_context* playback_context, box_rendercmd* rendercmd) {
	BX_ASSERT(renderer_backend != NULL && playback_context != NULL && rendercmd != NULL && "Invalid arguments passed to box_renderer_backend_submit_rendercmd");

#if BOX_ENABLE_VALIDATION
	if (!rendercmd->finished) {
		BX_ERROR("box_renderer_backend_submit_rendercmd(): Tried to submit rendercmd before ending commands.");
		return FALSE;
	}
#endif

	u8* cursor = 0;
	while (freelist_next_block(&rendercmd->buffer, &cursor)) {
		rendercmd_header* hdr = (rendercmd_header*)cursor;
		BX_ASSERT(hdr->type <= RENDERCMD_END && "Malformed data in box_rendercmd");

		rendercmd_payload* payload = (rendercmd_payload*)(cursor + sizeof(rendercmd_header));

		if (hdr->supported_mode)
			playback_context->current_mode = hdr->supported_mode;

		switch (hdr->type) {
		case RENDERCMD_BIND_RENDERTARGET:
#if BOX_ENABLE_VALIDATION
			if (playback_context->current_target != NULL) {
				BX_ERROR("Submission validation: Tried to bind rendertarget twice in box_rendercmd.");
				return FALSE;
			}
#endif

			playback_context->current_target = payload->bind_rendertarget.rendertarget;
			break;

		case RENDERCMD_BEGIN_RENDERSTAGE:
#if BOX_ENABLE_VALIDATION
			if (playback_context->current_shader != NULL) {
				BX_ERROR("Submission validation: Tried to begin renderstage twice in box_rendercmd.");
				return FALSE;
			}
#endif

			playback_context->current_shader = payload->begin_renderstage.renderstage;
			break;

		case RENDERCMD_END_RENDERSTAGE:
#if BOX_ENABLE_VALIDATION
			if (playback_context->current_shader == NULL) {
				BX_ERROR("Submission validation: Tried to end renderstage twice in box_rendercmd.");
				return FALSE;
			}
#endif

			playback_context->current_shader = NULL;
			break;

		case RENDERCMD_DRAW:
		case RENDERCMD_DRAW_INDEXED:
		case RENDERCMD_DISPATCH:
#if BOX_ENABLE_VALIDATION
			if (playback_context->current_shader == NULL) {
				BX_ERROR("Submission validation: Tried to dispatch draw call without a renderstage in box_rendercmd.");
				return FALSE;
			}
#endif
			break;
		}

		renderer_backend->execute_command(renderer_backend, playback_context, hdr, payload);
	}

	return TRUE;
}
