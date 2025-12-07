#pragma once

#include "defines.h"

#include "resource_system.h"

// Hitchbacks on resource system
// 

typedef enum box_shader_stage_type {
    BOX_SHADER_STAGE_TYPE_VERTEX,
    BOX_SHADER_STAGE_TYPE_GEOMETRY,
    BOX_SHADER_STAGE_TYPE_FRAGMENT,
    BOX_SHADER_STAGE_TYPE_COMPUTE,
    BOX_SHADER_STAGE_TYPE_MAX,
} box_shader_stage_type;

typedef struct shader_stage {
    const void* file_data;
    u64 file_size;
    void* internal_context;
} shader_stage;

// Container for shader stages to later be connected to a renderstage.
typedef struct box_renderstage {
    box_resource_header header;
    shader_stage stages[BOX_SHADER_STAGE_TYPE_MAX];

    void* internal_data;
} box_renderstage;

// Container for buffer of data stored on GPU.
typedef struct box_renderbuffer {
    box_resource_header header;

    void* internal_data;
} box_renderbuffer;

box_renderstage* box_engine_create_renderstage(struct box_engine* engine, const char* shader_stages[], u8 shader_stages_count, struct box_vertex_layout* layout, box_renderbuffer* vertex_buffer, box_renderbuffer* index_buffer, b8 depth_test, b8 blending);