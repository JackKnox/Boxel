#include "voxel_model.h"

#include "platform/filesystem.h"

b8 load_model_vox_format(box_resource_system* system, voxel_model* model, void* arg) {
	file_handle* handle = (file_handle*)arg;

	char header[4];
	if (!filesystem_read(handle, sizeof(header), header, NULL) ||
		!bcmp_memory(header, "VOX ", sizeof(header))) {
		BX_ERROR("Invalid VOX file: Cannot read header.");
		return FALSE;
	}
	
	u32 version = 0;
	if (!filesystem_read(handle, sizeof(version), &version, NULL)) {
		BX_ERROR("Invalid VOX file: Cannot read version.");
		return FALSE;
	}

	char chunk_id[5];
	u32 main_chunk_size = 0;
	u32 main_child_size = 0;
	
	// Read the MAIN chunk header
    if (!filesystem_read(handle, sizeof(chunk_id) - 1, chunk_id, NULL) || 
		!bcmp_memory(chunk_id, "MAIN", sizeof(chunk_id) - 1) ||
		!filesystem_read(handle, sizeof(u32), &main_chunk_size, NULL) ||
		!filesystem_read(handle, sizeof(u32), &main_child_size, NULL)) {
        BX_ERROR("Invalid VOX file: Missing / invalid MAIN chunk.");
        return FALSE;
    }

    // Skip the MAIN chunk content (usually empty)
    filesystem_seek(handle, main_chunk_size, FILE_SEEK_CURRENT);

	u64 file_end = filesystem_tell(handle) + main_child_size;
	while (filesystem_tell(handle) < file_end) {
		if (!filesystem_read(handle, sizeof(chunk_id) - 1, chunk_id, NULL))
			break;
		chunk_id[4] = '\0';

		u32 chunk_size = 0, child_size = 0;
        if (!filesystem_read(handle, sizeof(u32), &chunk_size, NULL) ||
            !filesystem_read(handle, sizeof(u32), &child_size, NULL)) {
            BX_ERROR("Invalid VOX file: Invalid chunk header for '%s'.", chunk_id);
            return FALSE;
        }

		if (bcmp_memory(chunk_id, "SIZE", sizeof(chunk_id))) {
			 if (!filesystem_read(handle, sizeof(uvec3), &model->model_size, NULL)) {
                BX_ERROR("Failed to read SIZE chunk.");
                return FALSE;
            }

            u64 count = (u64)model->model_size.x * model->model_size.y * model->model_size.z;
            model->file_data = (voxel_material*)ballocate(count * sizeof(voxel_material), MEMORY_TAG_RESOURCES);

            // Skip padding if any
            filesystem_seek(handle, chunk_size - sizeof(uvec3), FILE_SEEK_CURRENT);
		}
		else if (bcmp_memory(chunk_id, "XYZI", sizeof(chunk_id))) {
			u32 num_voxels = 0;
            if (!filesystem_read(handle, sizeof(u32), &num_voxels, NULL)) {
				BX_ERROR("Invalid VOX file: Failed to read XYZI chunk.");
				return FALSE;
			}

            for (u32 i = 0; i < num_voxels; ++i) {
                u8 v[4];
                if (!filesystem_read(handle, sizeof(v), v, NULL)) {
					BX_ERROR("Invalid VOX file: Failed to read pixel at index '%i'.", i);
					return FALSE;
				}

                u32 index = (v[2] * model->model_size.y * model->model_size.x) + (v[1] * model->model_size.x) + v[0];
                model->file_data[index].colour.palette_index = v[3] - 1; // palette index is 1-based
            }
		}
		else if (bcmp_memory(chunk_id, "RGBA", sizeof(chunk_id))) {
            if (!filesystem_read(handle, sizeof(model->palette), model->palette, NULL)) {
				BX_ERROR("Invalid VOX file: Failed to read RGBA chunk.");
				return FALSE;
			}
		}
		else {
            // Skip unrecognized chunk
            filesystem_seek(handle, chunk_size + child_size, FILE_SEEK_CURRENT);
		}
		
	}

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