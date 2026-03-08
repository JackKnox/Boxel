#include "defines.h"
#include "renderer_backend.h"

#include "platform/filesystem.h"
#include "utils/string_utils.h"

#include "render_objects.h"

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

box_renderbuffer box_renderbuffer_default() {
    box_renderbuffer buffer = {};
	
	return buffer;
}

box_renderstage box_renderstage_graphics_default(const char **shader_stages, u8 shader_stage_count) {
    box_renderstage stage = {};
	stage.mode = RENDERER_MODE_GRAPHICS;
	
	if (!collect_renderstage_shaders(&stage, shader_stage_count, shader_stages))
		return stage;
	
    return stage;
}

box_renderstage box_renderstage_compute_default(const char **shader_stages, u8 shader_stage_count) {
    box_renderstage stage = {};
	stage.mode = RENDERER_MODE_COMPUTE;
	
	if (!collect_renderstage_shaders(&stage, shader_stage_count, shader_stages))
		return stage;
	
    return stage;
}


box_texture box_texture_default() {
	box_texture texture = {};
	texture.address_mode = BOX_ADDRESS_MODE_REPEAT;
	texture.filter_type = BOX_FILTER_TYPE_NEAREST;

	return texture;
}

u64 box_texture_get_size_in_bytes(box_texture* texture) {
	if (!texture) return 0;
	return texture->size.x * texture->size.y * texture->image_format.channel_count;
}

box_rendertarget box_rendertarget_default() {
    box_rendertarget rendertarget = {};
    return rendertarget;
}