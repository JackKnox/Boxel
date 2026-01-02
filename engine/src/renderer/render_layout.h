#pragma once

#include "defines.h"

#include "renderer/renderer_backend.h"

// Supported format types for the renderer backend.
typedef enum box_format_type {
	BOX_FORMAT_TYPE_UINT8,
	BOX_FORMAT_TYPE_UINT16,
	BOX_FORMAT_TYPE_UINT32,

	BOX_FORMAT_TYPE_SINT8,
	BOX_FORMAT_TYPE_SINT16,
	BOX_FORMAT_TYPE_SINT32,

	BOX_FORMAT_TYPE_FLOAT32,
	BOX_FORMAT_TYPE_SRGB,

	BOX_FORMAT_TYPE_BOOL,
} box_format_type;

// Structure for format of general data in renderer backend.
typedef struct box_render_format {
	box_format_type type;
	u8 channel_count;
	b8 normalized;
} box_render_format;

// Supported usages for a renderbuffer in host or local.
typedef enum box_renderbuffer_usage {
	BOX_RENDERBUFFER_USAGE_VERTEX = 1 << 0,
	BOX_RENDERBUFFER_USAGE_INDEX = 1 << 1,
	BOX_RENDERBUFFER_USAGE_STORAGE = 1 << 2,
} box_renderbuffer_usage;

// Supported descriptor / uniform types for renderer backend.
typedef enum box_descriptor_type {
	BOX_DESCRIPTOR_TYPE_STORAGE_BUFFER,
	BOX_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	BOX_DESCRIPTOR_TYPE_IMAGE_SAMPLER,
} box_descriptor_type;

// Supported topologys for vertex buffers.
typedef enum box_vertex_topology_type {
	BOX_VERTEX_TOPOLOGY_TRIANGLES,
	BOX_VERTEX_TOPOLOGY_POINTS,
	BOX_VERTEX_TOPOLOGY_LINES,
} box_vertex_topology_type;

typedef struct box_vertex_attrib_desc {
	box_render_format type;
	u32 offset;
} box_vertex_attrib_desc;

typedef struct box_descriptor_desc {
	box_descriptor_type descriptor_type;
	box_shader_stage_type stage_type;
} box_descriptor_desc;

#define BOX_MAX_VERTEX_ATTRIBS 8
#define BOX_MAX_DESCRIPTORS 8

// Describes layout of data used by the renderer backend.
typedef struct box_render_layout {
	box_vertex_attrib_desc attribs[BOX_MAX_VERTEX_ATTRIBS];
	box_descriptor_desc descriptors[BOX_MAX_DESCRIPTORS];
	box_format_type index_type;
	box_vertex_topology_type topology_type;

	u64 stride;

	u8 attrib_count, descriptor_count;
	b8 initialized;
} box_render_layout;

// Size lookup for attribute types.
u64 box_render_format_size(box_render_format type);

// Adds a vertex attribute to the layout. This function does not allow specifying normalization.
void box_render_layout_add(box_render_layout* layout, box_render_format format);

// Adds a descriptor to the descriptor set later to be created along with the renderstage.
void box_render_layout_add_descriptor(box_render_layout* layout, box_descriptor_type descriptor_type, box_shader_stage_type stage_type);

// Set the topology of the layout to be applied to connected buffers.
void box_render_layout_set_topology(box_render_layout* layout, box_vertex_topology_type topology);

// Set the index type of the later applied index buffer.
void box_render_layout_set_index_type(box_render_layout* layout, box_format_type type);

// Finalizes the render layout after all attributes have been added. After calling this function, the layout becomes immutable.
void box_render_layout_end(box_render_layout* layout);

// Get total stride from render layout.
u64 box_render_layout_stride(box_render_layout* layout);

// Get count of total attributes from render layout.
u32 box_render_layout_count(box_render_layout* layout);