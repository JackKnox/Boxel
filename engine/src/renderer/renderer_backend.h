#pragma once

#include "defines.h"

typedef enum renderer_backend_type {
    RENDERER_BACKEND_TYPE_VULKAN,
    RENDERER_BACKEND_TYPE_OPENGL,
    RENDERER_BACKEND_TYPE_DIRECTX
} renderer_backend_type;

struct box_config;

typedef struct renderer_backend {
    void* internal_context;

    b8 (*initialize)(struct renderer_backend* backend, struct box_config* config);

    void (*shutdown)(struct renderer_backend* backend);

    void (*resized)(struct renderer_backend* backend, u16 width, u16 height);

    b8 (*begin_frame)(struct renderer_backend* backend, f32 delta_time);
    b8 (*end_frame)(struct renderer_backend* backend);
} renderer_backend;

b8 renderer_backend_create(renderer_backend_type type, renderer_backend* out_renderer_backend);
void renderer_backend_destroy(renderer_backend* renderer_backend);