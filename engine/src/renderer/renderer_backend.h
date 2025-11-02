#pragma once

#include "defines.h"

// Type for Graphics API/Method.
typedef enum renderer_backend_type {
    RENDERER_BACKEND_TYPE_VULKAN,
    RENDERER_BACKEND_TYPE_OPENGL,
    RENDERER_BACKEND_TYPE_DIRECTX
} renderer_backend_type;

typedef struct renderer_capabilities {
    i32 graphics_queue_index;
    i32 present_queue_index;
    i32 transfer_queue_index;
    i32 compute_queue_index;
} renderer_capabilities;

// Configuration for render backend.
typedef struct renderer_backend_config {
    const char* application_name;

    // Chosen type of API.
    renderer_backend_type api_type;

    /// Enabled queues for backend to use.
    b8 graphics, compute, transfer, present;

    // Enable three buffers on swapchain.
    b8 use_triple_buffering;

    // Send validation messages through out engine.
    b8 enable_validation;

    // Enable sampler anisotropy on GPU.
    b8 sampler_anisotropy;

    // Force renderer backend to use a discrete GPU.
    b8 discrete_gpu;

    // darray - Extensions enabled on graphics API.
    const char** required_extensions;

    // Output of renderer backend.
    renderer_capabilities capabilities;
} renderer_backend_config;

struct box_engine;
struct box_platform;

typedef struct renderer_backend {
    void* internal_context;
    struct box_platform* plat_state;

    b8 (*initialize)(struct renderer_backend* backend, renderer_backend_config* config);

    void (*shutdown)(struct renderer_backend* backend);

    void (*resized)(struct renderer_backend* backend, u16 width, u16 height);

    b8 (*begin_frame)(struct renderer_backend* backend, f32 delta_time);
    b8 (*end_frame)(struct renderer_backend* backend);
} renderer_backend;

b8 renderer_backend_create(renderer_backend_type type, struct box_platform* plat_state, renderer_backend* out_renderer_backend);
void renderer_backend_destroy(renderer_backend* renderer_backend);