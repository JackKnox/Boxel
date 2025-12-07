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

b8 internal_create_renderstage(box_resource_system* system, box_renderstage* resource, box_engine* engine) {
	return engine->renderer.create_internal_renderstage(&engine->renderer, resource);
}

void internal_destroy_renderstage(box_resource_system* system, box_renderstage* resource, box_engine* engine) {
	engine->renderer.destroy_internal_renderstage(&engine->renderer, resource);
}

box_renderstage* box_engine_create_renderstage(
	box_engine* engine, 
	const char* shader_stages[], u8 shader_stages_count, 
	box_vertex_layout* layout, 
	box_renderbuffer* vertex_buffer, box_renderbuffer* index_buffer, 
	b8 depth_test, b8 blending) {
	box_renderstage* renderstage = NULL;
	if (!resource_system_allocate_resource(&engine->resource_system, sizeof(box_renderstage), &renderstage)) {
		// Error already printed...
		return NULL;
	}

	u32 success_stages = 0;
	for (int i = 0; i < shader_stages_count; ++i) {
		shader_stage* stage = &renderstage->stages[get_stage_type_from_filepath(shader_stages[i])];
		
		stage->file_data = filesystem_read_entire_binary_file(shader_stages[i], &stage->file_size);
		if (!stage->file_data) {
			BX_WARN("Unable to read binary: %s.", shader_stages[i]);
			continue;
		}

		++success_stages;
	}
	
	if (success_stages <= 0) {
		BX_ERROR("No successfully loaded shaders attached to box_renderstage.");
		return NULL;
	}

	renderstage->header.vtable.create_local = internal_create_renderstage;
	renderstage->header.vtable.destroy_local = internal_destroy_renderstage;
	renderstage->header.resource_arg = (void*)engine;
	resource_system_signal_upload(&engine->resource_system, renderstage);
	return renderstage;
}