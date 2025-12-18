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

typedef enum box_renderbuffer_usage {
    BOX_RENDERBUFFER_USAGE_VERTEX,
    BOX_RENDERBUFFER_USAGE_INDEX,
} box_renderbuffer_usage;

typedef struct shader_stage {
    const void* file_data;
    u64 file_size;
} shader_stage;

// Container for buffer of data stored on GPU.
typedef struct box_renderbuffer {
    box_resource_header header;
    box_renderbuffer_usage usage;
    void* temp_user_data;
    u64 temp_user_size;

    void* internal_data;
} box_renderbuffer;

// Container for shader stages to later be connected to a renderstage.
typedef struct box_renderstage {
    box_resource_header header;
    shader_stage stages[BOX_SHADER_STAGE_TYPE_MAX];
    box_renderbuffer* vertex_buffer, *index_buffer;
    struct box_vertex_layout* layout;
    b8 depth_test, blending;

    void* internal_data;
} box_renderstage;


// Creates a renderstage asynchronously and logs it with the resource system attached to the specified box_engine.
box_renderstage* box_engine_create_renderstage(struct box_engine* engine, const char* shader_stages[], u8 shader_stages_count, box_renderbuffer* vertex_buffer, box_renderbuffer* index_buffer, struct box_vertex_layout* layout, b8 depth_test, b8 blending);

// Create a buffer on the GPU asynchronously and logs it with the resource system attached to the specified box_engine.
box_renderbuffer* box_engine_create_renderbuffer(struct box_engine* engine, box_renderbuffer_usage usage, void* data_to_send, u64 data_size);