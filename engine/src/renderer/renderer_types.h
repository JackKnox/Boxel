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
typedef struct box_shader {
    box_resource_header header;
    shader_stage stages[BOX_SHADER_STAGE_TYPE_MAX];
} box_shader;

// Container for buffer of data stored on GPU.
typedef struct box_renderbuffer {
    box_resource_header header;

    void* internal_data;
} box_renderbuffer;

box_shader* box_engine_create_shader(struct box_engine* engine, const char* shader_stages[], u8 shader_stages_count);