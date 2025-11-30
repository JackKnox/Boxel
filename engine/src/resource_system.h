#pragma once

#include "defines.h"

#include "utils/freelist.h"

typedef enum box_resource_type {
	BOX_RESOURCE_TYPE_NONE = 0,
	BOX_RESOURCE_TYPE_SHADER,
	BOX_RESOURCE_TYPE_RENDERBUFFER,
	//BOX_RESOURCE_TYPE_OCTREE,
	//BOX_RESOURCE_TYPE_MODEL,
} box_resource_type;

typedef enum box_resource_state {
	BOX_RESOURCE_STATE_UNINITIALIZED,
	BOX_RESOURCE_STATE_NEEDS_UPLOAD,  // main thread requested upload
	BOX_RESOURCE_STATE_UPLOADING,     // render/upload thread is processing
	BOX_RESOURCE_STATE_READY,         // GPU resource created & usable
	BOX_RESOURCE_STATE_FAILED,        // upload/create failed
	BOX_RESOURCE_STATE_PENDING_DELETE // flagged for deletion when safe
} box_resource_state;

typedef struct box_resource_header {
	box_resource_type type;
	box_resource_state state;
	u64 handle;
} box_resource_header;

typedef struct box_resource_system {
	freelist resources;
} box_resource_system;

b8 box_engine_new_resource(struct box_engine* engine, b8 is_renderer_resource, void* resource, u64 resource_size);

void box_engine_destroy_resources(struct box_engine* engine);

void box_engine_prepare_resources(struct box_engine* engine);