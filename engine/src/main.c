#include "engine.h"

int main(int argc, char** argv)
{
	box_config app_config = box_default_config();
	box_engine* engine = box_create_engine(&app_config);

	if (!engine)
	{
		BX_INFO("Boxel initialisation failed");
		return 1;
	}

	BX_INFO("Boxel initializated successully");

	while (box_engine_is_running(engine))
	{
		box_rendercmd command = {0};
		box_rendercmd_set_clear_colour(&command, 0.1f, 0.1f, 0.1f);

		box_engine_render_frame(engine, &command);
	}

	box_destroy_engine(engine);
	return 0;
}