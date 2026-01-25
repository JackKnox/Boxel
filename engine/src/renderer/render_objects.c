#include "defines.h"
#include "renderer_backend.h"

#include "platform/filesystem.h"
#include "utils/string_utils.h"

#include "engine_private.h"

box_shader_stage_type get_stage_type_from_filepath(const char* filepath) {
	if (string_find_substr(filepath, ".vert.spv"))
		return BOX_SHADER_STAGE_TYPE_VERTEX;
	else if (string_find_substr(filepath, ".frag.spv"))
		return BOX_SHADER_STAGE_TYPE_FRAGMENT;
	else if (string_find_substr(filepath, ".comp.spv"))
		return BOX_SHADER_STAGE_TYPE_COMPUTE;

	BX_ASSERT(FALSE && "Unsupported shader stage type!");
	return 0;
}

box_renderer_mode box_stage_type_to_renderer_mode(box_shader_stage_type stage_type) {
	switch (stage_type) {
	case BOX_SHADER_STAGE_TYPE_VERTEX:   return RENDERER_MODE_GRAPHICS;
	case BOX_SHADER_STAGE_TYPE_FRAGMENT: return RENDERER_MODE_GRAPHICS;
	case BOX_SHADER_STAGE_TYPE_COMPUTE:  return RENDERER_MODE_COMPUTE;
	}

	BX_ASSERT(FALSE && "Unsupported shader stage type!");
	return 0;
}

b8 internal_create_renderstage(box_renderstage* resource, box_engine* engine) {
	BX_ASSERT(resource != NULL && engine != NULL && "Invalid arguments passed to internal_create_renderstage");
	return engine->renderer.create_internal_renderstage(&engine->renderer, resource);
}

void internal_destroy_renderstage(box_renderstage* resource, box_engine* engine) {
	BX_ASSERT(resource != NULL && engine != NULL && "Invalid arguments passed to internal_destroy_renderstage");
	engine->renderer.destroy_internal_renderstage(&engine->renderer, resource);
}

b8 collect_renderstage_shaders(box_renderstage* renderstage, u32 shader_stage_count, const char** shader_stages) {
	u32 success_stages = 0;
	for (int i = 0; i < shader_stage_count; ++i) {
		box_shader_stage_type stage_type = get_stage_type_from_filepath(shader_stages[i]);
		box_renderer_mode mode = box_stage_type_to_renderer_mode(stage_type);

		if (renderstage->mode == 0) renderstage->mode = mode;
#if BOX_ENABLE_VALIDATION
		else if (renderstage->mode != mode) {
			BX_ERROR("Mixing shader stages in renderstage creation");
			continue;
		}
#endif

		shader_stage* stage = &renderstage->stages[stage_type];

		stage->file_data = filesystem_read_entire_binary_file(shader_stages[i], &stage->file_size);
		if (!stage->file_data) {
			BX_WARN("Unable to read binary: %s", shader_stages[i]);
			continue;
		}

		++success_stages;
	}
	
	return (success_stages > 0);
}

box_renderstage* box_engine_create_graphics_stage(
	box_engine* engine, 
	box_render_layout* layout, 
	const char** shader_stages, u32 shader_stage_count, 
	box_vertex_topology_type topology, 
	box_renderbuffer* vertex_buffer, box_renderbuffer* index_buffer) {
#if BOX_ENABLE_VALIDATION
	// Validate mandatory inputs
	if (!engine || !layout || !shader_stages || shader_stage_count <= 0) {
		BX_ERROR("Invalid arguments when creating graphics stage");
		return NULL;
	}

	// Validate buffer consistency
	if (index_buffer && !vertex_buffer) {
		BX_ERROR("Cannot create graphics stage with an index buffer but no vertex buffer");
		return NULL;
	}
#endif

	box_renderstage* renderstage = resource_system_allocate_resource(engine->resource_system, sizeof(box_renderstage));
	if (!renderstage) {
    	BX_ERROR("Failed to allocate memory for box_renderstage");
		return NULL;
	}

	if (!collect_renderstage_shaders(renderstage, shader_stage_count, shader_stages)) {
		BX_ERROR("No successfully loaded shaders attached to renderstage");
		return NULL;
	}

	// Fill static data
	if (layout) renderstage->layout = *layout;
	renderstage->graphics.vertex_buffer = vertex_buffer;
	renderstage->graphics.index_buffer = index_buffer;

	renderstage->header.vtable.create_local = internal_create_renderstage;
	renderstage->header.vtable.destroy_local = internal_destroy_renderstage;
	renderstage->header.resource_arg = (void*)engine;
	resource_system_signal_upload(engine->resource_system, renderstage);
	return renderstage;
}

box_renderstage* box_engine_create_compute_stage(
	box_engine *engine, 
	box_render_layout *layout, 
	const char **shader_stages, 
	u32 shader_stage_count) {
#if BOX_ENABLE_VALIDATION
	// Validate mandatory inputs
	if (!engine || !layout || !shader_stages || shader_stage_count <= 0) {
		BX_ERROR("Invalid arguments when creating compute stage");
		return NULL;
	}
#endif

	box_renderstage* renderstage = resource_system_allocate_resource(engine->resource_system, sizeof(box_renderstage));
	if (!renderstage) {
    	BX_ERROR("Failed to allocate memory for box_renderstage");
		return NULL;
	}

	if (!collect_renderstage_shaders(renderstage, shader_stage_count, shader_stages)) {
		BX_ERROR("No successfully loaded shaders attached to renderstage");
		return NULL;
	}

	// Fill static data
	renderstage->layout = *layout;

	renderstage->header.vtable.create_local = internal_create_renderstage;
	renderstage->header.vtable.destroy_local = internal_destroy_renderstage;
	renderstage->header.resource_arg = (void*)engine;
	resource_system_signal_upload(engine->resource_system, renderstage);
	return renderstage;
}

b8 internal_create_renderbuffer(box_renderbuffer* resource, box_engine* engine) {
	BX_ASSERT(resource != NULL && engine != NULL && "Invalid arguments passed to internal_create_renderbuffer");
	b8 result = engine->renderer.create_internal_renderbuffer(&engine->renderer, resource);
	
	if (resource->temp_user_data != NULL) {
		result = (result && engine->renderer.upload_to_renderbuffer(&engine->renderer, resource, resource->temp_user_data, 0, resource->buffer_size));

		bfree(resource->temp_user_data, resource->buffer_size, MEMORY_TAG_RESOURCES);
		resource->temp_user_data = NULL;
	}
	return result; 
}

void internal_destroy_renderbuffer(box_renderbuffer* resource, box_engine* engine) {
	BX_ASSERT(resource != NULL && engine != NULL && "Invalid arguments passed to internal_destroy_renderbuffer");
	engine->renderer.destroy_internal_renderbuffer(&engine->renderer, resource);
}

box_renderbuffer *box_engine_create_renderbuffer(
    box_engine *engine,
    box_renderbuffer_usage usage,
    u64 buffer_size,
    const void *data_to_send)
{
#if BOX_ENABLE_VALIDATION
	if (usage == 0) {
		BX_ERROR("Not specified a renderbuffer usage to box_engine_create_renderbuffer");
		return NULL;
	}

	if (data_to_send != NULL && buffer_size == 0) {
		BX_ERROR("Specified a data ptr but no size to box_engine_create_renderbuffer");
		return NULL;
	}
#endif 

	box_renderbuffer* renderbuffer = resource_system_allocate_resource(engine->resource_system, sizeof(box_renderbuffer));
	if (!renderbuffer) return NULL;

	// Fill static data
	renderbuffer->usage = usage;
	renderbuffer->buffer_size = buffer_size;

	if (data_to_send) {
		renderbuffer->temp_user_data = ballocate(renderbuffer->buffer_size, MEMORY_TAG_RESOURCES);
		bcopy_memory(renderbuffer->temp_user_data, data_to_send, renderbuffer->buffer_size);
	}

	renderbuffer->header.vtable.create_local = internal_create_renderbuffer;
	renderbuffer->header.vtable.destroy_local = internal_destroy_renderbuffer;
	renderbuffer->header.resource_arg = (void*)engine;
	resource_system_signal_upload(engine->resource_system, renderbuffer);
	return renderbuffer;
}

b8 internal_create_texture(box_texture* resource, box_engine* engine) {
	BX_ASSERT(resource != NULL && engine != NULL && "Invalid arguments passed to internal_create_texture");
	b8 result = engine->renderer.create_internal_texture(&engine->renderer, resource);

	if (resource->temp_user_data != NULL) {
		bfree(resource->temp_user_data, box_texture_get_total_size(resource), MEMORY_TAG_RESOURCES);
		resource->temp_user_data = NULL;
	}
	return result;
}

void internal_destroy_texture(box_texture* resource, box_engine* engine) {
	BX_ASSERT(resource != NULL && engine != NULL && "Invalid arguments passed to internal_destroy_texture");
	engine->renderer.destroy_internal_texture(&engine->renderer, resource);
}

box_texture* box_engine_create_texture(
	box_engine* engine, 
	uvec2 size, 
	box_render_format image_format,
	box_filter_type filter_type,
	box_address_mode address_mode,
	const void* data) {
#if BOX_ENABLE_VALIDATION
	if (size.width == 0 || size.height == 0 || image_format.channel_count == 0) {
		BX_ERROR("Invalid format of data passed to box_engine_create_texture");
		return NULL;
	}
#endif
	
	box_texture* texture = resource_system_allocate_resource(engine->resource_system, sizeof(box_texture));
	if (!texture) return NULL;

	// Fill static data
	texture->size = size;
	texture->image_format = image_format;
	texture->filter_type = filter_type;
	texture->address_mode = address_mode;

	if (data) {
		u64 total_size = box_texture_get_total_size(texture);

		texture->temp_user_data = ballocate(total_size, MEMORY_TAG_RESOURCES);
		bcopy_memory(texture->temp_user_data, data, total_size);
	}

	texture->header.vtable.create_local = internal_create_texture;
	texture->header.vtable.destroy_local = internal_destroy_texture;
	texture->header.resource_arg = (void*)engine;
	resource_system_signal_upload(engine->resource_system, texture);
	return texture;
}

u64 box_texture_get_total_size(box_texture* texture) {
	if (!texture) return 0;
	return texture->size.x * texture->size.y * texture->image_format.channel_count;
}