#include "defines.h"
#include "renderer_types.h"

#include "platform/filesystem.h"
#include "resource_system.h"
#include "vertex_layout.h"

// TODO: AHHHHHHHHHH!
#include "engine_private.h"

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

b8 internal_create_shader(box_resource_system* system, box_shader* resource, box_engine* engine) {
	return engine->renderer.create_internal_shader(&engine->renderer, resource);
}

void internal_destroy_shader(box_resource_system* system, box_shader* resource, box_engine* engine) {
	engine->renderer.destroy_internal_shader(&engine->renderer, resource);
}

box_shader* box_engine_create_shader(
	box_engine* engine, 
	const char* shader_stages[], 
	u8 shader_stages_count) {
	box_shader* shader_ptr = platform_allocate(sizeof(box_shader), FALSE);
	platform_zero_memory(shader_ptr, sizeof(box_shader));

	int success_stages = 0;
	for (int i = 0; i < shader_stages_count; ++i) {
		shader_stage* stage = &shader_ptr->stages[get_stage_type_from_filepath(shader_stages[i])];
		
		stage->file_data = filesystem_read_entire_binary_file(shader_stages[i], &stage->file_size);
		if (!stage->file_data) {
			BX_WARN("Unable to read binary: %s.", shader_stages[i]);
			continue;
		}

		++success_stages;
	}
	
	if (success_stages <= 0) {
		// Error already printed...
		return NULL;
	}

	shader_ptr->header.vtable.create_local = internal_create_shader;
	shader_ptr->header.vtable.destroy_local = internal_destroy_shader;
	if (!resource_system_new_resource(&engine->resource_system, shader_ptr, sizeof(box_shader), (void*)engine)) {
		// Error already printed...
		return NULL;
	}

	return shader_ptr;
}