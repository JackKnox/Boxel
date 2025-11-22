#include "defines.h"

// Async container of commands to send render backend.
typedef struct box_rendercmd {
	u32 clear_colour;
} box_rendercmd;

void box_rendercmd_set_clear_colour(box_rendercmd* cmd, f32 clear_r, f32 clear_g, f32 clear_b);