#include "defines.h"
#include "renderer_cmd.h"

#include "engine.h"

#include "renderer/renderer_types.h"
#include "renderer/render_layout.h"

#if BOX_ENABLE_VALIDATION
#   define CHECK_FINISHED() if (cmd->finished) { BX_ERROR("Adding to render command after ending, users should not be calling _box_rendercmd_end."); return; }
#else
#   define CHECK_FINISHED()
#endif

rendercmd_payload* add_command(box_rendercmd* cmd, renderer_mode mode, rendercmd_payload_type type, u64 payload_size) {
    BX_ASSERT(cmd != NULL && "Invalid arguments passed to add_command");

    u64 user_block_size = sizeof(rendercmd_header) + payload_size;

    if (freelist_empty(&cmd->buffer)) {
        freelist_create(user_block_size, MEMORY_TAG_RENDERER, &cmd->buffer);
    }
    
    rendercmd_header header = {
        .type = type,
        .supported_mode = mode,
    };

    void* user_memory = freelist_push(&cmd->buffer, user_block_size, &header);
    if (!user_memory) {
        BX_ERROR("Render command buffer out of memory!");
        return NULL;
    }

    // user_memory points at rendercmd_header
    return payload_size > 0 ? (rendercmd_payload*)((u8*)user_memory + sizeof(rendercmd_header)) : NULL;
}

void box_rendercmd_reset(box_rendercmd* cmd) {
    BX_ASSERT(cmd != NULL && "Invalid arguments passed to box_rendercmd_reset");
    if (!freelist_empty(&cmd->buffer))
        freelist_reset(&cmd->buffer, FALSE, FALSE);

    cmd->finished = FALSE;
}

void box_rendercmd_destroy(box_rendercmd* cmd) {
    BX_ASSERT(cmd != NULL && "Invalid arguments passed to box_rendercmd_destroy");
    freelist_destroy(&cmd->buffer);
    bzero_memory(cmd, sizeof(box_rendercmd));
}

void box_rendercmd_set_clear_colour(box_rendercmd* cmd, f32 clear_r, f32 clear_g, f32 clear_b) {
    if (!cmd) return;
    CHECK_FINISHED();

    rendercmd_payload* payload;
    payload = add_command(cmd, RENDERER_MODE_GRAPHICS, RENDERCMD_SET_CLEAR_COLOUR, sizeof(payload->set_clear_colour));
    payload->set_clear_colour.clear_colour =
        ((u32)(clear_r * 255.0f + 0.5f) << 24) |
        ((u32)(clear_g * 255.0f + 0.5f) << 16) |
        ((u32)(clear_b * 255.0f + 0.5f) << 8)  |
         (u32)(1.0f * 255.0f + 0.5f);
}

void box_rendercmd_begin_renderstage(box_rendercmd* cmd, box_renderstage* renderstage) {
    if (!cmd) return;
    CHECK_FINISHED();

    rendercmd_payload* payload;
    payload = add_command(cmd, renderstage->mode, RENDERCMD_BEGIN_RENDERSTAGE, sizeof(payload->begin_renderstage));
    payload->begin_renderstage.renderstage = renderstage;
}

void box_rendercmd_set_descriptor(box_rendercmd* cmd, void* value, u32 size, u32 binding) {
    if (!cmd) return;
    CHECK_FINISHED();

    rendercmd_payload* payload;
    payload = add_command(cmd, 0, RENDERCMD_SET_DESCRIPTOR, sizeof(payload->set_descriptor));
    payload->set_descriptor.value = value;
    payload->set_descriptor.size = size;
    payload->set_descriptor.binding = binding;
}

void box_rendercmd_draw(box_rendercmd* cmd, u32 vertex_count, u32 instance_count) {
    if (!cmd) return;
    CHECK_FINISHED();

    rendercmd_payload* payload;
    payload = add_command(cmd, RENDERER_MODE_GRAPHICS, RENDERCMD_DRAW, sizeof(payload->draw));
    payload->draw.vertex_count = vertex_count;
    payload->draw.instance_count = instance_count;
}

void box_rendercmd_draw_indexed(box_rendercmd* cmd, u32 index_count, u32 instance_count) {
    if (!cmd) return;
    CHECK_FINISHED();

    rendercmd_payload* payload;
    payload = add_command(cmd, RENDERER_MODE_GRAPHICS, RENDERCMD_DRAW_INDEXED, sizeof(payload->draw_indexed));
    payload->draw_indexed.index_count = index_count;
    payload->draw_indexed.instance_count = instance_count;
}

void box_rendercmd_dispatch(box_rendercmd* cmd, u32 group_size_x, u32 group_size_y, u32 group_size_z) {
    if (!cmd) return;
    CHECK_FINISHED();

    rendercmd_payload* payload;
    payload = add_command(cmd, RENDERER_MODE_COMPUTE, RENDERCMD_DISPATCH, sizeof(payload->draw));
    payload->dispatch.group_size.x = group_size_x;
    payload->dispatch.group_size.y = group_size_y;
    payload->dispatch.group_size.z = group_size_z;
}

void box_rendercmd_end_renderstage(box_rendercmd* cmd) {
    if (!cmd) return;
    CHECK_FINISHED();

    add_command(cmd, 0, RENDERCMD_END_RENDERSTAGE, 0);
}

void _box_rendercmd_end(box_rendercmd* cmd) {
    if (!cmd) return;
    CHECK_FINISHED();

    add_command(cmd, 0, _RENDERCMD_END, 0);
    cmd->finished = TRUE;
}
