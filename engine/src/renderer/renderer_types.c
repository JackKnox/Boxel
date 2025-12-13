#include "defines.h"
#include "renderer_types.h"

#include "platform/filesystem.h"
#include "resource_system.h"
#include "vertex_layout.h"

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
	box_renderbuffer* vertex_buffer, box_renderbuffer* index_buffer, 
	box_vertex_layout* layout,
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

	// Fill static data
	renderstage->layout = layout;
	renderstage->vertex_buffer = vertex_buffer;
	renderstage->index_buffer = index_buffer;
	renderstage->depth_test = depth_test;
	renderstage->blending = blending;

	renderstage->header.vtable.create_local = internal_create_renderstage;
	renderstage->header.vtable.destroy_local = internal_destroy_renderstage;
	renderstage->header.resource_arg = (void*)engine;
	resource_system_signal_upload(&engine->resource_system, renderstage);
	return renderstage;
}

b8 internal_create_renderbuffer(box_resource_system* system, box_renderbuffer* resource, box_engine* engine) {
	return engine->renderer.create_internal_renderbuffer(&engine->renderer, resource);
}

void internal_destroy_renderbuffer(box_resource_system* system, box_renderbuffer* resource, box_engine* engine) {
	engine->renderer.destroy_internal_renderbuffer(&engine->renderer, resource);
}

box_renderbuffer* box_engine_create_renderbuffer(box_engine* engine, void* data_to_send, u64 data_size) {
	box_renderbuffer* renderbuffer = NULL;
	if (!resource_system_allocate_resource(&engine->resource_system, sizeof(box_renderbuffer), &renderbuffer)) {
		// Error already printed...
		return NULL;
	}

	// Fill static data
	renderbuffer->temp_user_data = data_to_send;
	renderbuffer->temp_user_size = data_size;

	renderbuffer->header.vtable.create_local = internal_create_renderbuffer;
	renderbuffer->header.vtable.destroy_local = internal_destroy_renderbuffer;
	renderbuffer->header.resource_arg = (void*)engine;
	resource_system_signal_upload(&engine->resource_system, renderbuffer);
	return renderbuffer;
}
