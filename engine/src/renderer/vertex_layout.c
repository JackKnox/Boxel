#include "defines.h"
#include "vertex_layout.h"

u64 box_vertex_attrib_type_size(box_vertex_attrib_type type) {
    switch (type) {
	case BOX_VERTEX_ATTRIB_SINT8:
	case BOX_VERTEX_ATTRIB_UINT8:
	case BOX_VERTEX_ATTRIB_BOOL:
		return 1;
	case BOX_VERTEX_ATTRIB_SINT16:
	case BOX_VERTEX_ATTRIB_UINT16:
		return 2;
	case BOX_VERTEX_ATTRIB_SINT32:
	case BOX_VERTEX_ATTRIB_UINT32:
	case BOX_VERTEX_ATTRIB_FLOAT32:
		return 4;
	case BOX_VERTEX_ATTRIB_SINT64:
	case BOX_VERTEX_ATTRIB_UINT64:
	case BOX_VERTEX_ATTRIB_FLOAT64:
		return 8;
	default:
		return 1;
    }
}

void box_vertex_layout_add(box_vertex_layout* layout, box_vertex_attrib_type attrib_type, u64 num_count) {
	if (layout->initialized) {
		BX_WARN("Cannot add more attributes after ending vertex layout.");
		return;
	}

	if (layout->attrib_count + 1 >= BOX_MAX_VERTEX_ATTRIBS) {
		BX_WARN("Reached maximum attributes in vertex layout (%i).", BOX_MAX_VERTEX_ATTRIBS);
	}

	box_vertex_attrib_desc* desc = &layout->attribs[layout->attrib_count];
	desc->type = attrib_type;
	desc->count = num_count;
	layout->attrib_count++;
}

void box_vertex_layout_set_topology(box_vertex_layout* layout, box_vertex_topology_type topology) {
	if (layout->initialized) {
		BX_WARN("Cannot change vertex layout after ending.");
		return;
	}

	layout->topology_type = topology;
}

void box_vertex_layout_end(box_vertex_layout* layout) {
	u64 stride = 0;
	for (u32 i = 0; i < layout->attrib_count; ++i) {
		box_vertex_attrib_desc* desc = &layout->attribs[i];

		desc->offset = stride;
		stride += box_vertex_attrib_type_size(desc->type);
	}
}

// Get total stride from vertex layout.
u64 box_vertex_layout_stride(box_vertex_layout* layout) {
	if (!layout->initialized) {
		BX_WARN("Cannot retrieve vertex_layout count or stride before ending descriptors");
		return 0;
	}

	return layout->stride;
}

// Get count of total attributes from vertex layout.
u32 box_vertex_layout_count(box_vertex_layout* layout) {
	if (!layout->initialized) {
		BX_WARN("Cannot retrieve vertex_layout count or stride before ending descriptors");
		return 0;
	}

	return layout->attrib_count;
}