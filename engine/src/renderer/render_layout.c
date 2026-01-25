#include "defines.h"
#include "renderer_backend.h"

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

#if BOX_ENABLE_VALIDATION
#   define CHECK_INITIALIZED() if (layout->initialized) { BX_ERROR("Adding to render layout after ending."); bxdebug_break(); return; }
#else
#   define CHECK_INITIALIZED()
#endif

void box_render_layout_add(box_render_layout* layout, box_render_format attrib_type) {
	if (!layout) return;
	CHECK_INITIALIZED()

	if (layout->attrib_count + 1 >= BOX_MAX_VERTEX_ATTRIBS) {
		BX_WARN("Reached maximum attributes in render layout (%i).", BOX_MAX_VERTEX_ATTRIBS);
	}

	box_vertex_attrib_desc* desc = &layout->attribs[layout->attrib_count];
	desc->type = attrib_type;
	++layout->attrib_count;
}

void box_render_layout_add_descriptor(box_render_layout* layout, box_descriptor_type descriptor_type, box_shader_stage_type stage_type) {
	if (!layout) return;
	CHECK_INITIALIZED()
	
	box_descriptor_desc* desc = &layout->descriptors[layout->descriptor_count];
	desc->descriptor_type = descriptor_type;
	desc->stage_type = stage_type;
	++layout->descriptor_count;
}

void box_render_layout_end(box_render_layout* layout) {
	layout->stride = 0;
	for (u32 i = 0; i < layout->attrib_count; ++i) {
		box_vertex_attrib_desc* desc = &layout->attribs[i];

		desc->offset = layout->stride;
		layout->stride += box_render_format_size(desc->type);
	}

	layout->initialized = TRUE;
}

u64 box_render_layout_stride(box_render_layout* layout) {
	if (!layout) return 0;
	if (!layout->initialized) {
		BX_WARN("Cannot retrieve render layout stride before ending layout.");
		return 0;
	}

	return layout->stride;
}

u32 box_render_layout_count(box_render_layout* layout) {
	if (!layout) return 0;
	if (!layout->initialized) {
		BX_WARN("Cannot retrieve render layout count before ending layout.");
		return 0;
	}

	return layout->attrib_count;
}