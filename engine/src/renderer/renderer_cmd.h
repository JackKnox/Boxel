#pragma once

#include "defines.h"

/*
Memory layout:
void* buffer = buffer to render commands
u64 capacity = capacity of rendercmd buffer
u64 size = bytes used by rendercmd buffer

-- per command --
u32 command_type = type of render command
u64 payload_size = size of data associated with command
...rest is payload (size is payload_size)
*/

enum {
    RENDERCMD_SET_CLEAR_COLOUR,
    RENDERCMD_BEGIN_RENDERSTAGE,
    RENDERCMD_END_RENDERSTAGE,
    RENDERCMD_SET_VERTEX_BUFFER,
    RENDERCMD_DRAW,
};
typedef u32 rendercmd_payload_type;

// Async container of commands to send to the render backend.
typedef struct box_rendercmd {
    void* buffer;
    u64 capacity, size;
    b8 finished;
} box_rendercmd;

#pragma pack(push, 1)
typedef struct rendercmd_header {
    rendercmd_payload_type type;
    u64 payload_size;
} rendercmd_header;
#pragma pack(pop)

// Clears data with buffer without reallocating memory.
void box_rendercmd_reset(box_rendercmd* cmd);

// Set the clear color for the framebuffer. This color will be used when clearing the screen before rendering.
void box_rendercmd_set_clear_colour(box_rendercmd* cmd, f32 clear_r, f32 clear_g, f32 clear_b);

// Begin a new render stage with specified shaders. Subsequent draw calls will use this stage.
void box_rendercmd_begin_renderstage(box_rendercmd* cmd, const char* vertex_shader, const char* fragment_shader);

// Bind a vertex buffer for upcoming draw calls.
void box_rendercmd_set_vertex_buffer(box_rendercmd* cmd, struct box_vertexbuffer* vertex_buf);

// Issue a draw call with current bound state.
void box_rendercmd_draw(box_rendercmd* cmd, u32 vertex_count, u32 instance_count);

// End the current render stage and finalizes state.
void box_rendercmd_end_renderstage(box_rendercmd* cmd);

// Verify or create required GPU resources.This ensures that all shaders, buffers, and pipelines needed for this command are valid and ready for execution.
void box_rendercmd_verify_resources(box_rendercmd* cmd, struct box_engine* engine);