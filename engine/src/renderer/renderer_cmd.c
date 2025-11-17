#include "defines.h"
#include "renderer_cmd.h"

#include "engine.h"
#include "renderer/renderer_types.h"

void verify_size(box_rendercmd* cmd, u64 required_size) {
    if (cmd->capacity >= required_size) return;

    u64 new_size = BX_MAX(cmd->capacity * 2, required_size);
    if (new_size == 0) new_size = 1024;

    u8* new_buffer = platform_allocate(new_size, FALSE);

    if (cmd->buffer && cmd->size > 0) {
        platform_copy_memory(new_buffer, cmd->buffer, cmd->size);
        platform_free(cmd->buffer, FALSE);
    }

    cmd->buffer = new_buffer;
    cmd->capacity = new_size;
}

void add_commmand(box_rendercmd* cmd, rendercmd_type type, const void* payload, u64 size) {
    if (!cmd) return;

    const u64 header_size = sizeof(rendercmd_header);

    u64 start = cmd->size;
    u64 total = header_size + size;

    verify_size(cmd, start + total);
    u8* base = (u8*)cmd->buffer;

    // write header
    rendercmd_header hdr;
    hdr.type = type;
    hdr.payload_size = size;
    platform_copy_memory(base + start, &hdr, header_size);

    // write payload if present
    if (size > 0 && payload != NULL) {
        platform_copy_memory(base + start + header_size, payload, size);
    }

    // advance size to the end of the written command
    cmd->size = start + total;
}

void box_rendercmd_reset(box_rendercmd* cmd) {
    cmd->size = 0;

    if (cmd->buffer)
        platform_zero_memory(cmd->buffer, cmd->capacity);
}

void box_rendercmd_set_clear_colour(box_rendercmd* cmd, f32 clear_r, f32 clear_g, f32 clear_b) {
	if (!cmd) return;

    u32 payload =
        ((u32)(clear_r * 255.0f + 0.5f) << 24) |
        ((u32)(clear_g * 255.0f + 0.5f) << 16) |
        ((u32)(clear_b * 255.0f + 0.5f) << 8) |
        (u32)(1.0f * 255.0f + 0.5f);  // TODO: Specify alpha channel as well
    add_commmand(cmd, RENDERCMD_SET_CLEAR_COLOUR, &payload, sizeof(payload));
}

void box_rendercmd_begin_renderstage(box_rendercmd* cmd, const char* vertex_shader, const char* fragment_shader) {
    if (!cmd) return;

    // TOOD: Duplicate shader paths into buffer
    add_commmand(cmd, RENDERCMD_BEGIN_RENDERSTAGE, NULL, 0);
}

void box_rendercmd_set_vertex_buffer(box_rendercmd* cmd, box_vertexbuffer* vertex_buf) {
    if (!cmd) return;

    add_commmand(cmd, RENDERCMD_SET_VERTEX_BUFFER, &vertex_buf, sizeof(vertex_buf));
}

void box_rendercmd_draw(box_rendercmd* cmd, u32 vertex_count, u32 instance_count) {
    if (!cmd) return;

    typedef struct payload { u32 vertex_count; u32 instance_count; } payload;

    payload p;
    p.vertex_count = vertex_count;
    p.instance_count = instance_count;
    add_commmand(cmd, RENDERCMD_DRAW, &p, sizeof(p));
}

void box_rendercmd_end_renderstage(box_rendercmd* cmd) {
    if (!cmd) return;

    add_commmand(cmd, RENDERCMD_END_RENDERSTAGE, NULL, 0);
}

void box_rendercmd_verify_resources(box_rendercmd* cmd, box_engine* engine) {
    if (!cmd) return;

}
