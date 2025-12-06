#pragma once

#include "defines.h"

// Supported attribute data types for vertex buffers.
typedef enum box_vertex_attrib_type {
	BOX_VERTEX_ATTRIB_SINT8,
	BOX_VERTEX_ATTRIB_SINT16,
	BOX_VERTEX_ATTRIB_SINT32,
	BOX_VERTEX_ATTRIB_SINT64,

	BOX_VERTEX_ATTRIB_UINT8,
	BOX_VERTEX_ATTRIB_UINT16,
	BOX_VERTEX_ATTRIB_UINT32,
	BOX_VERTEX_ATTRIB_UINT64,

	BOX_VERTEX_ATTRIB_FLOAT32,
	BOX_VERTEX_ATTRIB_FLOAT64,

	BOX_VERTEX_ATTRIB_BOOL,
} box_vertex_attrib_type;

// Supported topologys for vertex buffers.
typedef enum box_vertex_topology_type {
	BOX_VERTEX_TOPOLOGY_TRIANGLES,
	BOX_VERTEX_TOPOLOGY_POINTS,
	BOX_VERTEX_TOPOLOGY_LINES,
} box_vertex_topology_type;

// Internal descriptor for each attribute to pass to renderer.
typedef struct box_vertex_attrib_desc {
	box_vertex_attrib_type type;
	u64 count;
	u64 offset;
	b8 normalized;
} box_vertex_attrib_desc;

#define BOX_MAX_VERTEX_ATTRIBS 16

// Describes a vertex layout used by the renderer.
typedef struct box_vertex_layout {
	box_vertex_attrib_desc attribs[BOX_MAX_VERTEX_ATTRIBS];
	u32 attrib_count;
	u64 stride;

	box_vertex_topology_type topology_type;
	b8 initialized;
} box_vertex_layout;

// Size lookup for attribute types.
u64 box_vertex_attrib_type_size(box_vertex_attrib_type type);

// Adds a vertex attribute to the layout. This function does not allow specifying normalization.
void box_vertex_layout_add(box_vertex_layout* layout, box_vertex_attrib_type attrib_type, u64 num_count);

// Set the topology of the layout to be applied to connected buffers.
void box_vertex_layout_set_topology(box_vertex_layout* layout, box_vertex_topology_type topology);

// Finalizes the vertex layout after all attributes have been added. After calling this function, the layout becomes immutable.
void box_vertex_layout_end(box_vertex_layout* layout);

// Get total stride from vertex layout.
u64 box_vertex_layout_stride(box_vertex_layout* layout);

// Get count of total attributes from vertex layout.
u32 box_vertex_layout_count(box_vertex_layout* layout);