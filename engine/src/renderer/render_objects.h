// Resources for renderer
// Should include box_renderstage, box_renderbuffer, box_texture
#pragma once

#include "defines.h"

#include "utils/freelist.h"

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

struct box_engine;

// Create a buffer on the GPU asynchronously and logs it with the resource system attached to the specified box_engine.
box_renderbuffer* box_engine_create_renderbuffer(struct box_engine* engine, box_renderbuffer_usage usage, u64 buffer_size, const void* data_to_send);

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

    union {
        struct {
            // Optional vertex buffer bound to this renderstage.
            box_renderbuffer* vertex_buffer;

            // Optional index buffer bound to this renderstage.
            box_renderbuffer* index_buffer;

            // Primitive topology used for rendering.
            box_vertex_topology_type topology_type;
        } graphics;

        struct {

        } compute;
    };

    // Vertex input and descriptor layout expected by the shaders.
    box_render_layout layout;
    
    // Backend-specific pipeline or program state.
    void* internal_data;

    // Rendering mode (graphics, compute, etc).
    box_renderer_mode mode;
} box_renderstage;

// Creates a renderstage with a graphics type asynchronously and logs it with the resource system attached to the specified box_engine.
box_renderstage* box_engine_create_graphics_stage(struct box_engine* engine, box_render_layout* layout, const char** shader_stages, u32 shader_stage_count, box_vertex_topology_type topology, box_renderbuffer* vertex_buffer, box_renderbuffer* index_buffer);

// Creates a renderstage with a compute type asynchronously and logs it with the resource system attached to the specified box_engine.
box_renderstage* box_engine_create_compute_stage(struct box_engine* engine, box_render_layout* layout, const char** shader_stages, u32 shader_stage_count);

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

// Create a texture in the resource system attached to the specified box_engine on the GPU asynchronously.
box_texture* box_engine_create_texture(struct box_engine* engine, uvec2 size, box_render_format image_format, box_filter_type filter_type, box_address_mode address_mode, const void* data);

// Gets total size of bytes storing internal texture data.
u64 box_texture_get_total_size(box_texture* texture);

// Async container of commands to send to the render backend.
// Written by the game thread and consumed by the render thread.
typedef struct {
    // Linear allocator backing the render command buffer.
    freelist buffer;

    // Indicates that no further commands may be written to this buffer.
    b8 finished;
} box_rendercmd;

// Clears data with buffer without reallocating memory.
void box_rendercmd_reset(box_rendercmd* cmd);

// Destroys and resets memory in render command
void box_rendercmd_destroy(box_rendercmd* cmd);

// Set the clear color for the framebuffer. This color will be used when clearing the screen before rendering.
void box_rendercmd_set_clear_colour(box_rendercmd* cmd, f32 clear_r, f32 clear_g, f32 clear_b);

// Begin a new render stage with specified shaders. Subsequent draw calls will use this stage.
void box_rendercmd_begin_renderstage(box_rendercmd* cmd, box_renderstage* renderstage);

// Issue a draw call with current bound state.
void box_rendercmd_draw(box_rendercmd* cmd, u32 vertex_count, u32 instance_count);

// Issue a indexed draw call with current bound state.
void box_rendercmd_draw_indexed(box_rendercmd* cmd, u32 index_count, u32 instance_count);

// Issue a compute dispatch with current bound state.
void box_rendercmd_dispatch(box_rendercmd* cmd, u32 group_size_x, u32 group_size_y, u32 group_size_z);

// End the current render stage and finalizes state.
void box_rendercmd_end_renderstage(box_rendercmd* cmd);

// Finalizes the render command buffer and prevents further modification.
// Intended for internal use by the engine and renderer backend.
void _box_rendercmd_end(box_rendercmd* cmd);