#pragma once

#include "defines.h"

typedef struct voxel_material {
	// Represents palette index when owned by model, 
	// but packed colour in octree (R8 G8 B8 A8).
	u32 packed_colour;
} voxel_material;

typedef struct octree_node {
	// Eight child per nodes: checks if child exists.
	u32 child_mask;

	// Index into packed darray for first node (+8 is end of node).
	u32 first_child;

	// Combined material for node.
	voxel_material material;
} octree_node;

// Packed interface into SVO.
typedef struct box_octree {
	// darray - packed array for nodes.
	octree_node* nodes;
} box_octree;

// Sets default for octree node.
octree_node box_default_octree_node();

// Loads supported formats into a box_octree and optimises.
b8 box_load_voxel_model(const char* filepath, box_octree* out_octree);

// Frees dynamic memory for octree model.
void box_destroy_voxel_model(box_octree* octree);