#pragma once

#include "defines.h"

#include "octree_gen.h"

#include "platform/filesystem.h"
#include "utils/math_types.h"

// Output of all voxel model loaders.
typedef struct voxel_model {
	// Version of file, if applicable.
	u32 file_version;

	// Dynamically allocated memory from file, must be freed by caller.
	voxel_material* file_data;

	// Size of model in 3d.
	uvec3 model_size;

	// Array to palette index within material.
	u32 palette[256];
} voxel_model;

b8 load_vox_format(file_handle* handle, voxel_model* out_model);