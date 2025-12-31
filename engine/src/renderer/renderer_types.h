#pragma once

#include "defines.h"

#include "renderer/render_layout.h"
#include "renderer/renderer_backend.h"

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
} shader_stage;

// Container for buffer of data stored on GPU.
typedef struct box_renderbuffer {
    box_resource_header header;
    box_renderbuffer_usage usage;
    u64 buffer_size;

    void* temp_user_data;

    void* internal_data;
} box_renderbuffer;

// Container for shader stages to later be connected to a renderstage.
typedef struct box_renderstage {
    box_resource_header header;
    shader_stage stages[BOX_SHADER_STAGE_TYPE_MAX];
    renderer_mode mode;
    box_render_layout layout;

    void* internal_data;
} box_renderstage;

typedef struct box_texture {
    box_resource_header header;
    box_render_format image_format;
    uvec2 size;

    void* temp_user_data;

    void* internal_data;
} box_texture;

// Creates a renderstage asynchronously and logs it with the resource system attached to the specified box_engine.
box_renderstage* box_engine_create_renderstage(struct box_engine* engine, box_render_layout* layout, u8 shader_stages_count, const char* shader_stages[]);

// Create a buffer on the GPU asynchronously and logs it with the resource system attached to the specified box_engine.
box_renderbuffer* box_engine_create_renderbuffer(struct box_engine* engine, box_renderbuffer_usage usage, u64 buffer_size, void* data_to_send);

// Create a texture in the resource system attached to the specified box_engine on the GPU asynchronously.
box_texture* box_engine_create_texture(struct box_engine* engine, uvec2 size, box_render_format image_format, const void* data);