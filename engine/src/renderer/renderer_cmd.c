#include "defines.h"
#include "renderer_cmd.h"

// TODO: Memory leaks!!

void grow_rendercmd(box_rendercmd* cmd, u64 total_size) {
    u64 required = cmd->size + total_size;

    // If first allocation or not enough space
    if (cmd->buffer == NULL || required > cmd->capacity) {

        // Grow exponentially to avoid constant reallocations
        u64 new_capacity = cmd->capacity == 0 ? 256 : cmd->capacity;
        while (new_capacity < required) {
            new_capacity *= 2;
        }

        void* new_buffer = platform_allocate(new_capacity, FALSE);

        if (cmd->buffer) {
            platform_copy_memory(new_buffer, cmd->buffer, cmd->size);
            platform_free(cmd->buffer, FALSE);
        }

        cmd->buffer = new_buffer;
        cmd->capacity = new_capacity;
    }
}

void add_commmand(box_rendercmd* cmd, rendercmd_payload_type type, const void* payload, u64 size) {
    rendercmd_header header;
    header.type = type;
    header.payload_size = size;

    u64 total_size = sizeof(header) + header.payload_size;
    grow_rendercmd(cmd, total_size);

    // Actually add payload
    u8* dest = (u8*)cmd->buffer + cmd->size;
    platform_copy_memory(dest, &header, sizeof(header));

    platform_copy_memory(dest + sizeof(header), payload, header.payload_size);
    cmd->size += total_size;
}

void box_rendercmd_reset(box_rendercmd* cmd) {
    cmd->size = 0;
}

void box_rendercmd_set_clear_colour(box_rendercmd* cmd, f32 clear_r, f32 clear_g, f32 clear_b) {
    u32 payload =
        ((u32)(clear_r * 255.0f + 0.5f) << 24) |
        ((u32)(clear_g * 255.0f + 0.5f) << 16) |
        ((u32)(clear_b * 255.0f + 0.5f) << 8) |
        (u32)(1.0f * 255.0f + 0.5f);  // TODO: Specify alpha channel as well
    add_commmand(cmd, RENDERCMD_SET_CLEAR_COLOUR, &payload, sizeof(payload));
}