#include "defines.h"
#include "renderer_cmd.h"

#include "engine.h"

// TODO: Memory leaks!!

void grow_rendercmd(box_rendercmd* cmd, u64 additional_size) {
    u64 required = cmd->size + additional_size;

    if (cmd->buffer == NULL || required > cmd->capacity) {
        u64 new_capacity = cmd->capacity == 0 ? 256 : cmd->capacity;
        while (new_capacity < required) new_capacity *= 2;

        void* new_buffer = platform_allocate(new_capacity, FALSE);
        if (cmd->buffer) {
            platform_copy_memory(new_buffer, cmd->buffer, cmd->size);
            platform_free(cmd->buffer, FALSE);
        }

        cmd->buffer = new_buffer;
        cmd->capacity = new_capacity;
    }
}

void add_command(box_rendercmd* cmd, rendercmd_payload_type type, const void* payload, u64 payload_size) {
    rendercmd_header header;
    header.type = type;
    header.payload_size = payload_size;

    u64 total_size = sizeof(rendercmd_header) + payload_size;
    grow_rendercmd(cmd, total_size);

    u8* dest = (u8*)cmd->buffer + cmd->size;
    platform_copy_memory(dest, &header, sizeof(rendercmd_header));

    if (payload_size > 0 && payload != NULL) {
        platform_copy_memory(dest + sizeof(rendercmd_header), payload, payload_size);
    }

    cmd->size += total_size;
}


void box_rendercmd_reset(box_rendercmd* cmd) {
    cmd->size = 0;
    cmd->finished = FALSE;
}

void box_rendercmd_set_clear_colour(box_rendercmd* cmd, f32 clear_r, f32 clear_g, f32 clear_b) {
    u32 payload =
        ((u32)(clear_r * 255.0f + 0.5f) << 24) |
        ((u32)(clear_g * 255.0f + 0.5f) << 16) |
        ((u32)(clear_b * 255.0f + 0.5f) << 8) |
        (u32)(1.0f * 255.0f + 0.5f);  // TODO: Specify alpha channel as well
    add_command(cmd, RENDERCMD_SET_CLEAR_COLOUR, &payload, sizeof(payload));
}

void box_rendercmd_begin_renderstage(box_rendercmd* cmd, const char* vertex_shader, const char* fragment_shader)
{
    add_command(cmd, RENDERCMD_BEGIN_RENDERSTAGE, NULL, 0);
}

void box_rendercmd_set_vertex_buffer(box_rendercmd* cmd, struct box_vertexbuffer* vertex_buf)
{
}

void box_rendercmd_draw(box_rendercmd* cmd, u32 vertex_count, u32 instance_count)
{
    typedef struct { u32 vertex_count; u32 instance_count; } draw_payload;

    draw_payload payload;
    payload.vertex_count = vertex_count;
    payload.instance_count = instance_count;
    add_command(cmd, RENDERCMD_DRAW, &payload, sizeof(payload));
}

void box_rendercmd_end_renderstage(box_rendercmd* cmd)
{
    add_command(cmd, RENDERCMD_END_RENDERSTAGE, NULL, 0);
}

void box_rendercmd_verify_resources(box_rendercmd* cmd, box_engine* engine)
{
}
