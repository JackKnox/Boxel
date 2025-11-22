#pragma once

#include "defines.h"

// Type for Graphics API/Method.
typedef enum renderer_backend_type {
    RENDERER_BACKEND_TYPE_VULKAN,
    RENDERER_BACKEND_TYPE_OPENGL,
    RENDERER_BACKEND_TYPE_DIRECTX
} renderer_backend_type;

typedef enum renderer_device_type {
    RENDERER_DEVICE_TYPE_OTHER = 0,
    RENDERER_DEVICE_TYPE_INTEGRATED_GPU = 1,
    RENDERER_DEVICE_TYPE_DISCRETE_GPU = 2,
    RENDERER_DEVICE_TYPE_VIRTUAL_GPU = 3,
    RENDERER_DEVICE_TYPE_CPU = 4,
} renderer_device_type;

typedef struct renderer_capabilities {
    i32 graphics_queue_index;
    i32 present_queue_index;
    i32 transfer_queue_index;
    i32 compute_queue_index;

    char device_name[256];
    renderer_device_type device_type;
} renderer_capabilities;

// Configuration for render backend.
typedef struct renderer_backend_config {
    // Chosen type of API.
    renderer_backend_type api_type;

    /// Enabled queues for backend to use.
    b8 graphics, compute, transfer, present;

    // Frame count for swapchain, must be greater than 1.
    u32 swapchain_frame_count;

    // Send validation messages through out engine.
    b8 enable_validation;

    // Enable sampler anisotropy on GPU.
    b8 sampler_anisotropy;

    // Force renderer backend to use a discrete GPU.
    b8 discrete_gpu;

    // darray - Extensions enabled on graphics API.
    const char** required_extensions;

    // NOTE: All variables below are exported from renderer backend.
    // 

    // Output of renderer backend.
    renderer_capabilities capabilities;

    // The framebuffer's current width.
    u32 framebuffer_width;

    // The framebuffer's current height.
    u32 framebuffer_height;
} renderer_backend_config;

typedef struct renderer_backend {
    void* internal_context;
    struct box_platform* plat_state;

    b8 (*initialize)(struct renderer_backend* backend, const char* application_name, renderer_backend_config* config);

    void (*shutdown)(struct renderer_backend* backend);

    void (*resized)(struct renderer_backend* backend, u32 width, u32 height);

    b8 (*begin_frame)(struct renderer_backend* backend, f32 delta_time);
    b8 (*playback_rendercmd)(struct renderer_backend* backend, struct box_rendercmd* rendercmd);
    b8 (*end_frame)(struct renderer_backend* backend);
} renderer_backend;

b8 renderer_backend_create(renderer_backend_type type, struct box_platform* plat_state, renderer_backend* out_renderer_backend);
void renderer_backend_destroy(renderer_backend* renderer_backend);