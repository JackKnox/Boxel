#include "defines.h"
#include "octree_gen.h"

#include "voxel_loader.h"

#include "utils/darray.h"
#include "platform/filesystem.h"

u32 octree_node_recursive(box_octree* octree, voxel_model* model, uvec3 origin, int size);

voxel_material* voxel_get(const voxel_model* g, uvec3 position) {
	return &g->file_data[(position.x)+(position.y)*g->model_size.x + (position.z)*g->model_size.x * g->model_size.x];
}

// Check if region is uniform (all same color) and return that color in out_color
b8 region_uniform_colour(const voxel_model* g, uvec3 position, int size, voxel_material* out_color) {
	voxel_material* c0 = voxel_get(g, position);
	for (int z = position.z; z < position.z + size; ++z)
		for (int y = position.y; y < position.y + size; ++y)
			for (int x = position.x; x < position.x + size; ++x)
				if (voxel_get(g, position) != c0) return 0;
	if (out_color) *out_color = *c0;
	return 1;
}

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

	// Find cube side length (power of two) to contain model
	int max_dim = model.model_size.x;
	if (model.model_size.y > max_dim) max_dim = model.model_size.y;
	if (model.model_size.z > max_dim) max_dim = model.model_size.z;
	int cube = 1;
	while (cube < max_dim) cube <<= 1;

	out_octree->nodes = darray_create(octree_node);
	u32 root_index = octree_node_recursive(out_octree, &model, (uvec3){ 0, 0, 0 }, cube);

	platform_free(model.file_data, FALSE);
	return TRUE;
}

void box_destroy_voxel_model(box_octree* octree) {
	darray_destroy(octree->nodes);
}

u32 octree_node_recursive(box_octree* octree, voxel_model* model, uvec3 origin, int size) {
	voxel_material uniform_colour = {0};
	if (region_uniform_colour(model, origin, size, &uniform_colour)) {
		// create leaf node (empty or filled)
		octree_node node = box_default_octree_node();
		node.material = uniform_colour;

		darray_push(octree->nodes, &node);
		return (u32)darray_length(octree->nodes) - 1;
	}

	octree_node placeholder = box_default_octree_node();
	u32 my_index = (u32)darray_length(octree->nodes);
	darray_push(octree->nodes, &placeholder);

	u32 first_child_idx = (u32)darray_length(octree->nodes);
	u32 child_mask = 0;

	int half = size / 2;
    for (int i = 0; i < 8; ++i) {
        int dx = (i & 1) ? half : 0;
        int dy = (i & 2) ? half : 0;
        int dz = (i & 4) ? half : 0;
        int cx = origin.x + dx;
        int cy = origin.y + dy;
        int cz = origin.z + dz;

        // If child region all empty -> skip creating child (sparse)
		voxel_material uniform_empty = {0};
        if (region_uniform_colour(model, origin, half, &uniform_empty) && uniform_empty.packed_colour == 0) {
            continue;
        } 
		else {
            // create child
            u32 cidx = octree_node_recursive(octree, model, origin, half);
            child_mask |= (1u << i);
        }
    }

	// if we ended up with no children (shouldn't happen since we tested uniform earlier),
	// mark as empty leaf
	if (child_mask == 0) {
		// make this a leaf empty
		octree_node leaf = box_default_octree_node();

		// replace placeholder
		octree->nodes[my_index] = leaf;
		return my_index;
	}

	// update placeholder with child info
	octree->nodes[my_index].child_mask = child_mask;
	octree->nodes[my_index].first_child = first_child_idx;
	octree->nodes[my_index].material.packed_colour = 0; // internal nodes have material 0 by default
	return my_index;
}