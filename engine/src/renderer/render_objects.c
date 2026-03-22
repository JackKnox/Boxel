#include "defines.h"
#include "renderer_backend.h"

#include "platform/filesystem.h"
#include "utils/string_utils.h"

#include "render_objects.h"

box_renderbuffer_config box_renderbuffer_default() {
    box_renderbuffer_config config = {};
	return config;
}

box_graphicstage_config box_renderstage_default_graphic() {
	box_graphicstage_config config = {};
    return config;
}

box_computestage_config box_renderstage_default_compute() {
	box_computestage_config config = {};
    return config;
}

box_texture_config box_texture_default() {
    box_texture_config config = {};
	config.address_mode = BOX_ADDRESS_MODE_REPEAT;
	config.filter_type = BOX_FILTER_TYPE_NEAREST;
	config.usage = BOX_TEXTURE_USAGE_SAMPLED;

	return config;
}

u64 box_texture_get_size_in_bytes(box_texture* texture) {
	BX_ASSERT(texture != NULL && "Invalid arguments passed box_texture_get_size_in_bytes");
	return texture->size.x * texture->size.y *  box_render_format_size(texture->image_format);
}

box_rendertarget_config box_rendertarget_default() {
    box_rendertarget_config config = {};
    return config;
}