#include "defines.h"
#include "renderer_cmd.h"

void box_rendercmd_set_clear_colour(box_rendercmd* cmd, f32 clear_r, f32 clear_g, f32 clear_b) {
	cmd->clear_r = clear_r;
	cmd->clear_g = clear_g;
	cmd->clear_b = clear_b;
}