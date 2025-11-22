#include "defines.h"
#include "renderer_cmd.h"

void box_rendercmd_set_clear_colour(box_rendercmd* cmd, f32 clear_r, f32 clear_g, f32 clear_b) {
    cmd->clear_colour =
        ((u32)(clear_r * 255.0f + 0.5f) << 24) |
        ((u32)(clear_g * 255.0f + 0.5f) << 16) |
        ((u32)(clear_b * 255.0f + 0.5f) << 8) |
        (u32)(1.0f * 255.0f + 0.5f);  // TODO: Specify alpha channel as well
}