#pragma once

#include "engine.h"
#include "resource_system.h"

typedef struct {
	// Represents palette index when owned by model, 
	// but packed colour in octree (R8 G8 B8 A8).
	union {
		// Index to voxel_model::palette
		u32 palette_index;

		// Must be in layout R8B8G8A8
		i32 packed_colour;
	} colour;
} voxel_material;

// Output of all voxel model loaders.
typedef struct {
	box_resource_header header;

	// Dynamically allocated memory from file.
	voxel_material* file_data;

	// Size of model in 3D space.
	uvec3 model_size;

	// Array to palette index within material.
	u32 palette[256];
} voxel_model;

voxel_model* voxel_model_create(box_engine* engine, const char* filepath);