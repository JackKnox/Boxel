#pragma once

#include "defines.h"

#include "utils/freelist.h"

#include "renderer_backend.h"

/*
Memory layout:
void* buffer = buffer to render commands
u64 capacity = capacity of rendercmd buffer
u64 size = bytes used by rendercmd buffer

-- per command --
u64 payload_size = size of data associated with command
u32 command_type = type of render command
...rest is payload (size is payload_size)
*/

enum {
    RENDERCMD_SET_CLEAR_COLOUR,
    RENDERCMD_BEGIN_RENDERSTAGE,
    RENDERCMD_END_RENDERSTAGE,
    RENDERCMD_SET_DESCRIPTOR,
    RENDERCMD_DRAW,
    RENDERCMD_DRAW_INDEXED,
    RENDERCMD_DISPATCH,

    // Private command used for benefit of the renderer backend for ending state.
    _RENDERCMD_END,
};
typedef u32 rendercmd_payload_type;

// Async container of commands to send to the render backend.
typedef struct box_rendercmd {
    freelist buffer;
    b8 finished;
} box_rendercmd;

// Holds the current render state during playback.
typedef struct box_rendercmd_context {
    struct box_renderstage* current_shader;

    // TODO: Remove!
    struct vulkan_command_buffer* command_buffer;
} box_rendercmd_context;

#pragma pack(push, 1)
typedef struct rendercmd_header {
    rendercmd_payload_type type;
    renderer_mode supported_mode;
} rendercmd_header;
#pragma pack(pop)

#pragma pack(push, 1)
typedef union rendercmd_payload {
    struct {
        u32 clear_colour;
    } set_clear_colour;

    struct {
        struct box_renderstage* renderstage;
    } begin_renderstage;

    struct {
        void* value;
        u32 size, binding;
    } set_descriptor;

    struct {
        u32 vertex_count;
        u32 instance_count;
    } draw;

    struct {
        u32 index_count;
        u32 instance_count;
    } draw_indexed;

    struct {
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
void box_rendercmd_begin_renderstage(box_rendercmd* cmd, struct box_renderstage* renderstage);

// Sets descriptor / uniform at binding to value. Accepts box_texture*, box_renderbuffer* and any primitive type also supported by shader.
void box_rendercmd_set_descriptor(box_rendercmd* cmd, void* value, u32 size_of_value, u32 binding);

// Issue a draw call with current bound state.
void box_rendercmd_draw(box_rendercmd* cmd, u32 vertex_count, u32 instance_count);

// Issue a indexed draw call with current bound state.
void box_rendercmd_draw_indexed(box_rendercmd* cmd, u32 index_count, u32 instance_count);

// Issue a compute dispatch with current bound state.
void box_rendercmd_dispatch(box_rendercmd* cmd, u32 group_size_x, u32 group_size_y, u32 group_size_z);

// End the current render stage and finalizes state.
void box_rendercmd_end_renderstage(box_rendercmd* cmd);

// End the current render command and makes set render command immutable.
void _box_rendercmd_end(box_rendercmd* cmd);