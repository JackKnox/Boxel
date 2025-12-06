#pragma once

#include "defines.h"

#include "utils/freelist.h"

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
    freelist buffer;
    b8 finished;
} box_rendercmd;

#pragma pack(push, 1)
typedef struct rendercmd_header {
    rendercmd_payload_type type;
} rendercmd_header;
#pragma pack(pop)


#pragma pack(push, 1)
typedef union rendercmd_payload {
    struct {
        u32 clear_colour;
    } set_clear_colour;

    struct {
        u32 vertex_count;
        u32 instance_count;
    } draw;

    struct {
        struct box_shader* shader;
        struct box_vertex_layout* layout;
        struct box_renderbuffer* vertex_buffer, *index_buffer;
        b8 depth_test, blending;
    } begin_renderstage;

} rendercmd_payload;
#pragma pack(pop)

// Clears data with buffer without reallocating memory.
void box_rendercmd_reset(box_rendercmd* cmd);

// Destroys and resets memory in render command
void box_rendercmd_destroy(box_rendercmd* cmd);

// Set the clear color for the framebuffer. This color will be used when clearing the screen before rendering.
void box_rendercmd_set_clear_colour(box_rendercmd* cmd, f32 clear_r, f32 clear_g, f32 clear_b);

// Begin a new render stage with specified shaders. Subsequent draw calls will use this stage.
void box_rendercmd_begin_renderstage(box_rendercmd* cmd, struct box_shader* shader, struct box_vertex_layout* layout, struct box_renderbuffer* vertex_buffer, struct box_renderbuffer* index_buffer, b8 depth_test, b8 blending);

// Issue a draw call with current bound state.
void box_rendercmd_draw(box_rendercmd* cmd, u32 vertex_count, u32 instance_count);

// End the current render stage and finalizes state.
void box_rendercmd_end_renderstage(box_rendercmd* cmd);