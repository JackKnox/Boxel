#include "defines.h"
#include "octree_gen.h"

#include "voxel_loader.h"

#include "utils/darray.h"
#include "platform/filesystem.h"

octree_node box_default_octree_node() {
	octree_node node = { 0 };
	node.child_mask = 0;
	node.first_child = 0xFFFFFFFFu;
	node.material.packed_colour = 0;
	return node;
}

b8 box_load_voxel_model(const char* filepath, box_octree* out_octree) {
	file_handle handle;
	if (!filesystem_open(filepath, FILE_MODE_READ, TRUE, &handle)) {
		BX_ERROR("Could not open voxel model file: %s.", filepath);
		return FALSE;
	}

	voxel_model model = {0};
	platform_set_memory(model.palette, 0xFFFFFFFF, sizeof(model.palette));

	b8 load_result = FALSE;
	if (strstr(filepath, ".vox"))
		load_result = load_vox_format(&handle, &model);

	if (!load_result) {
		BX_ERROR("Failed to read model format: %s.", filepath);
		return FALSE;
	}

	return TRUE;
}

void box_destroy_voxel_model(box_octree* octree) {
	darray_destroy(octree->nodes);
}