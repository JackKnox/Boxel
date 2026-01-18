#pragma once

#include "defines.h"

#include "utils/freelist.h"

#include "renderer_backend.h"
#include "renderer/renderer_types.h"

// These describe high-level rendering intent and are translated into backend-specific API calls (OpenGL, Vulkan, etc).
enum {
    RENDERCMD_SET_CLEAR_COLOUR,
    RENDERCMD_BEGIN_RENDERSTAGE,
    RENDERCMD_END_RENDERSTAGE,
    RENDERCMD_SET_DESCRIPTOR,
    RENDERCMD_DRAW,
    RENDERCMD_DRAW_INDEXED,
    RENDERCMD_DISPATCH,

    // Private command used internally by the renderer backend to finalize state.
    _RENDERCMD_END,
};
typedef u32 rendercmd_payload_type;

// Async container of commands to send to the render backend.
// Written by the game thread and consumed by the render thread.
typedef struct {
    // Linear allocator backing the render command buffer.
    freelist buffer;

    // Indicates that no further commands may be written to this buffer.
    b8 finished;
} box_rendercmd;

#pragma pack(push, 1)
// Header prepended to every render command payload.
typedef struct rendercmd_header {
    // Type of render command.
    rendercmd_payload_type type;

    // Renderer mode this command is valid for (graphics / compute).
    renderer_mode supported_mode;
} rendercmd_header;
#pragma pack(pop)

#pragma pack(push, 1)
// Payload data for all supported render commands.
// Interpreted based on rendercmd_payload_type.
typedef union rendercmd_payload {
    struct {
        // Packed RGBA clear colour.
        u32 clear_colour;
    } set_clear_colour;

    struct {
        // Renderstage to bind for subsequent draw or dispatch calls.
        struct box_renderstage* renderstage;
    } begin_renderstage;

    // Descriptor or resource binding update.
    resource_binding set_descriptor;

    struct {
        // Number of vertices to draw.
        u32 vertex_count;

        // Number of instances to draw.
        u32 instance_count;
    } draw;

    struct {
        // Number of indices to draw.
        u32 index_count;

        // Number of instances to draw.
        u32 instance_count;
    } draw_indexed;

    struct {
        // Compute workgroup dimensions.
        uvec3 group_size;
    } dispatch;

} rendercmd_payload;
#pragma pack(pop)

// Clears data with buffer without reallocating memory.
void box_rendercmd_reset(box_rendercmd* cmd);

// Destroys and resets memory in render command
void box_rendercmd_destroy(box_rendercmd* cmd);

// Set the clear color for the framebuffer. This color will be used when clearing the screen before rendering.
void box_rendercmd_set_clear_colour(box_rendercmd* cmd, f32 clear_r, f32 clear_g, f32 clear_b);

// Begin a new render stage with specified shaders. Subsequent draw calls will use this stage.
void box_rendercmd_begin_renderstage(box_rendercmd* cmd, box_renderstage* renderstage);

// Sets descriptor / uniform at binding to specified texture.
void box_rendercmd_set_texture(box_rendercmd* cmd, box_texture* texture, u32 binding);

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