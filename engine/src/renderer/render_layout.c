#include "defines.h"
#include "render_layout.h"

u64 box_render_data_type_size(box_render_data_type type) {
    switch (type) {
	case BOX_RENDER_DATA_TYPE_SINT8:
	case BOX_RENDER_DATA_TYPE_UINT8:
	case BOX_RENDER_DATA_TYPE_BOOL:
		return 1;
	case BOX_RENDER_DATA_TYPE_SINT16:
	case BOX_RENDER_DATA_TYPE_UINT16:
		return 2;
	case BOX_RENDER_DATA_TYPE_SINT32:
	case BOX_RENDER_DATA_TYPE_UINT32:
	case BOX_RENDER_DATA_TYPE_FLOAT32:
		return 4;
	default:
		return 1;
    }
}

void box_render_layout_add(box_render_layout* layout, box_render_data_type attrib_type, u32 num_count) {
	if (layout->initialized) {
		BX_WARN("Cannot add more attributes after ending render layout.");
		return;
	}

	if (layout->attrib_count + 1 >= BOX_MAX_VERTEX_ATTRIBS) {
		BX_WARN("Reached maximum attributes in render layout (%i).", BOX_MAX_VERTEX_ATTRIBS);
	}

	box_vertex_attrib_desc* desc = &layout->attribs[layout->attrib_count];
	desc->type = attrib_type;
	desc->count = num_count;
	++layout->attrib_count;
}

void box_render_layout_add_descriptor(box_render_layout* layout, box_renderbuffer_usage buffer_type) {
	if (layout->initialized) {
		BX_WARN("Cannot add more descriptors after ending render layout.");
		return;
	}

	layout->descriptors[layout->descriptor_count] = buffer_type;
	++layout->descriptor_count;
}

void box_render_layout_set_topology(box_render_layout* layout, box_vertex_topology_type topology) {
	if (layout->initialized) {
		BX_WARN("Cannot set topology after ending render layout.");
		return;
	}

	layout->topology_type = topology;
}

void box_render_layout_set_index_type(box_render_layout* layout, box_render_data_type type) {
	if (layout->initialized) {
		BX_WARN("Cannot set index type after ending render layout.");
		return;
	}

	layout->index_type = type;
}

void box_render_layout_end(box_render_layout* layout) {
	layout->stride = 0;
	for (u32 i = 0; i < layout->attrib_count; ++i) {
		box_vertex_attrib_desc* desc = &layout->attribs[i];

		desc->offset = layout->stride;
		layout->stride += box_render_data_type_size(desc->type) * desc->count;
	}

	layout->initialized = TRUE;
}

// Get total stride from vertex layout.
u64 box_render_layout_stride(box_render_layout* layout) {
	if (!layout->initialized) {
		BX_WARN("Cannot retrieve render layout stride before ending layout.");
		return 0;
	}

	return layout->stride;
}

// Get count of total attributes from vertex layout.
u32 box_render_layout_count(box_render_layout* layout) {
	if (!layout->initialized) {
		BX_WARN("Cannot retrieve render layout count before ending layout.");
		return 0;
	}

	return layout->attrib_count;
}