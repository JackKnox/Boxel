#include "voxel_model.h"

#include "platform/filesystem.h"

b8 load_model_vox_format(box_resource_system* system, voxel_model* model, void* arg) {
	return TRUE;
}

void internal_destroy_model(box_resource_system* system, voxel_model* model, void* arg) {
	file_handle* handle = (file_handle*)arg;

	filesystem_close(handle);
	bfree(handle, sizeof(file_handle), MEMORY_TAG_RESOURCES);
	model->header.resource_arg = NULL;
}

voxel_model* voxel_model_create(box_engine* engine, const char* filepath) {
	box_resource_system* system = 
		box_engine_get_resource_system(engine);

	voxel_model* model = resource_system_allocate_resource(system, sizeof(voxel_model));
	if (!model) return NULL;

	bset_memory(model->palette, 0xFFFFFFFF, sizeof(model->palette));

	file_handle handle;
	if (!filesystem_open(filepath, FILE_MODE_READ, TRUE, &handle)) {
		BX_ERROR("Could not open voxel model file: %s.", filepath);
		return FALSE;
	}

	if (strstr(filepath, ".vox")) {
		model->header.vtable.create_local = load_model_vox_format;
	}
	else {
		BX_ERROR("Unknown file format for voxel model: '%s'.", filepath);
		return NULL;
	}

	model->header.vtable.destroy_local = internal_destroy_model;
	model->header.resource_arg = ballocate(sizeof(file_handle), MEMORY_TAG_RESOURCES);
	bcopy_memory(model->header.resource_arg, &handle, sizeof(file_handle));
	resource_system_signal_upload(system, model);
	return model;
}