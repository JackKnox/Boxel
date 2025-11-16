#pragma once

#include "defines.h"

// Opuaqe handle for a vertex buffer on the GPU.
typedef struct box_vertexbuffer box_vertexbuffer;

box_vertexbuffer* box_engine_create_vertexbuffer(struct box_engine* engine, const void* data, u64 size, struct box_vertex_layout* layout);