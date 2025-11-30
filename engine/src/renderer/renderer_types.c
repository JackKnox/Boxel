#include "defines.h"
#include "renderer_types.h"

#include "platform/filesystem.h"
#include "engine.h"

box_shader_stage_type get_stage_type_from_filepath(const char* filepath) {
	if (strstr(filepath, ".vert.spv"))
		return BOX_SHADER_STAGE_TYPE_VERTEX;
	else if (strstr(filepath, ".frag.spv"))
		return BOX_SHADER_STAGE_TYPE_FRAGMENT;
	else if (strstr(filepath, ".comp.spv"))
		return BOX_SHADER_STAGE_TYPE_COMPUTE;
	else if (strstr(filepath, ".geom.spv"))
		return BOX_SHADER_STAGE_TYPE_GEOMETRY;

	return 0;
}

// TODO: Memory leaks!!

box_shader* box_engine_create_shader(box_engine* engine, const char* stages[], u8 stage_count) {
	box_shader* shader_ptr = platform_allocate(sizeof(box_shader), FALSE);
	platform_zero_memory(shader_ptr, sizeof(box_shader));
	shader_ptr->header.type = BOX_RESOURCE_TYPE_SHADER;

	int success_stages = 0;
	for (int i = 0; i < stage_count; ++i) {
		shader_stage* stage = &shader_ptr->stages[get_stage_type_from_filepath(stages[i])];
		
		stage->file_data = filesystem_read_entire_binary_file(stages[i], &stage->file_size);
		if (!stage->file_data) {
			BX_WARN("Unable to read binary: %s.", stages[i]);
			continue;
		}

		++success_stages;
	}
	
	if (success_stages <= 0) {
		// Error already printed...
		return NULL;
	}

	if (!box_engine_new_resource(engine, TRUE, shader_ptr, sizeof(box_shader))) {
		// Error already printed...
		return NULL;
	}

	return shader_ptr;
}