#include "engine.h"

int main(int argc, char** argv)
{
	box_config app_config = box_default_config();
	box_engine* engine = box_create_engine(&app_config);

	BX_INFO("Boxel initializated successully");

	while (box_engine_is_running(engine))
	{
	}

	box_destroy_engine(engine);
}