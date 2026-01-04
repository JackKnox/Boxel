#pragma once

#include "defines.h"

#include "renderer/renderer_backend.h"

// Describes a single vertex attribute within a vertex buffer layout.
typedef struct {
	// Data format of the vertex attribute (e.g. vec2, vec3, normalized, etc).
	box_render_format type;

	// Byte offset from the start of the vertex.
	u32 offset;
} box_vertex_attrib_desc;

// Describes a single descriptor binding used by a shader.
typedef struct {
	// Type of descriptor (uniform buffer, sampler, storage buffer, etc).
	box_descriptor_type descriptor_type;
	
	// Shader stage(s) this descriptor is visible to.
	box_shader_stage_type stage_type;
} box_descriptor_desc;

#define BOX_MAX_VERTEX_ATTRIBS 8
#define BOX_MAX_DESCRIPTORS 8

// Describes the complete data layout consumed by the renderer backend.
typedef struct {
	// Vertex attribute descriptions in binding order.
	box_vertex_attrib_desc attribs[BOX_MAX_VERTEX_ATTRIBS];

	// Descriptor bindings used by the pipeline.
	box_descriptor_desc descriptors[BOX_MAX_DESCRIPTORS];

	// Index buffer format (if indexed rendering is used).
	box_format_type index_type;

	// Primitive topology used for rendering.
	box_vertex_topology_type topology_type;

	// Total size in bytes of a single vertex.
	u64 stride;

	// Number of active vertex attributes.
	u8 attrib_count;

	// Number of active descriptors.
	u8 descriptor_count;

	// Indicates whether this layout has been fully initialized and validated.
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