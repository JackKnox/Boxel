#include "defines.h"
#include "voxel_loader.h"

#include <string.h>

b8 check_vox_header(file_handle* handle) {
	char header[4];
	u32 version = 0;
	
	if (!filesystem_read(handle, sizeof(header), header, NULL) && 
		memcmp(header, "VOX", sizeof(header)) != 0) {
		BX_ERROR("Cannot read VOX format header.");
		return FALSE;
	}

	if (!filesystem_read(handle, sizeof(version), &version, NULL)) {
		BX_ERROR("Cannot read VOX format version.");
		return FALSE;
	}

	return TRUE;
}

b8 load_vox_format(file_handle* handle, voxel_model* out_model) {
    if (!check_vox_header(handle)) {
        BX_ERROR("Cannot open file using the VOX format.");
        return FALSE;
    }

    char chunk_id[5] = { 0 }; // +1 for null terminator
    u32 main_chunk_size = 0;
    u32 main_child_size = 0;

    // Read the MAIN chunk header
    if (!filesystem_read(handle, 4, chunk_id, NULL) || memcmp(chunk_id, "MAIN", 4) != 0) {
        BX_ERROR("Invalid VOX file: Missing MAIN chunk.");
        return FALSE;
    }

    filesystem_read(handle, sizeof(u32), &main_chunk_size, NULL);
    filesystem_read(handle, sizeof(u32), &main_child_size, NULL);

    // Skip the MAIN chunk content (usually empty)
    filesystem_seek(handle, main_chunk_size, FILE_SEEK_CURRENT);

    u64 file_end = filesystem_tell(handle) + main_child_size;
    while (filesystem_tell(handle) < file_end) {
        if (!filesystem_read(handle, 4, chunk_id, NULL))
            break;
        chunk_id[4] = '\0';

        u32 chunk_size = 0, child_size = 0;
        if (!filesystem_read(handle, sizeof(u32), &chunk_size, NULL) ||
            !filesystem_read(handle, sizeof(u32), &child_size, NULL)) {
            BX_ERROR("Could not read VOX chunk header for '%s'.", chunk_id);
            return FALSE;
        }

        if (memcmp(chunk_id, "SIZE", 4) == 0) {
            if (!filesystem_read(handle, sizeof(uvec3), &out_model->model_size, NULL)) {
                BX_ERROR("Failed to read SIZE chunk.");
                return FALSE;
            }

            u64 count = (u64)out_model->model_size.x * out_model->model_size.y * out_model->model_size.z;
            out_model->file_data = (voxel_material*)platform_allocate(count * sizeof(voxel_material), FALSE);
            platform_zero_memory(out_model->file_data, count * sizeof(voxel_material));

            // Skip padding if any
            filesystem_seek(handle, chunk_size - sizeof(uvec3), FILE_SEEK_CURRENT);
        }
        else if (memcmp(chunk_id, "XYZI", 4) == 0) {
            u32 num_voxels = 0;
            filesystem_read(handle, sizeof(u32), &num_voxels, NULL);

            for (u32 i = 0; i < num_voxels; ++i) {
                u8 v[4];
                filesystem_read(handle, 4, v, NULL);
                u32 index = (v[2] * out_model->model_size.y * out_model->model_size.x) + (v[1] * out_model->model_size.x) + v[0];
                out_model->file_data[index].packed_colour = out_model->palette[v[3] - 1]; // palette index is 1-based
            }
        }
        else if (memcmp(chunk_id, "RGBA", 4) == 0) {
            filesystem_read(handle, sizeof(out_model->palette), out_model->palette, NULL);
        }
        else {
            // Skip unrecognized chunk
            filesystem_seek(handle, chunk_size + child_size, FILE_SEEK_CURRENT);
        }
    }

    return TRUE;
}