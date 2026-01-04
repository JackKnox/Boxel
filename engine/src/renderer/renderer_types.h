#pragma once

#include "defines.h"

#include "renderer/render_layout.h"
#include "renderer/renderer_backend.h"

#include "resource_system.h"

// Container for a GPU-resident buffer resource. Acts as a backend-agnostic handle to vertex, index, uniform, or storage buffers.
typedef struct box_renderbuffer {
    box_resource_header header;

    // Intended usage of the buffer (vertex, index, uniform, etc).
    box_renderbuffer_usage usage;

    // Total size of the buffer in bytes.
    u64 buffer_size;

    // Optional pointer to CPU-side data used during asynchronous creation.
    void* temp_user_data;

    // Backend-specific buffer handle/state.
    void* internal_data;
} box_renderbuffer;

// Raw shader stage data loaded from disk or memory.
typedef struct {
    // Pointer to shader source or compiled bytecode.
    const void* file_data;

    // Size of the shader data in bytes.
    u64 file_size;
} shader_stage;

// Container for one or more shader stages that form a render pipeline.
typedef struct box_renderstage {
    box_resource_header header;

    // Shader stages indexed by box_shader_stage_type.
    shader_stage stages[BOX_SHADER_STAGE_TYPE_MAX];
    
    // Vertex input and descriptor layout expected by the shaders.
    box_render_layout layout;
    
    // Optional vertex buffer bound to this renderstage.
    box_renderbuffer* vertex_buffer;

    // Optional index buffer bound to this renderstage.
    box_renderbuffer* index_buffer;

    // Backend-specific pipeline or program state.
    void* internal_data;

    // Rendering mode (graphics, compute, etc).
    renderer_mode mode;
} box_renderstage;

// Container for GPU image data and sampling parameters.
// Textures are backend-agnostic and converted to API-specific image
// and sampler objects by the renderer backend.
typedef struct box_texture {
    box_resource_header header;

    // Dimensions of the texture in pixels.
    uvec2 size;
       
    // Format of the stored image data.
    box_render_format image_format;

    // Texture filtering mode.
    box_filter_type filter_type;

    // Texture address (wrap) mode.
    box_address_mode address_mode;

    // Optional pointer to CPU-side data used during asynchronous creation.
    void* temp_user_data;

    // Backend-specific image and sampler state.
    void* internal_data;
} box_texture;

// Creates a renderstage asynchronously and logs it with the resource system attached to the specified box_engine.
box_renderstage* box_engine_create_renderstage(struct box_engine* engine, box_render_layout* layout, u8 shader_stages_count, const char* shader_stages[], box_renderbuffer* vertex_buffer, box_renderbuffer* index_buffer);

// Create a buffer on the GPU asynchronously and logs it with the resource system attached to the specified box_engine.
box_renderbuffer* box_engine_create_renderbuffer(struct box_engine* engine, box_renderbuffer_usage usage, u64 buffer_size, void* data_to_send);

// Create a texture in the resource system attached to the specified box_engine on the GPU asynchronously.
box_texture* box_engine_create_texture(struct box_engine* engine, uvec2 size, box_render_format image_format, box_filter_type filter_type, box_address_mode address_mode, const void* data);

// Gets total size of bytes storing internal texture data
u64 box_texture_get_total_size(box_texture* texture);