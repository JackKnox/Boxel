#pragma once

#include "defines.h"

#include "renderer_types.h"

// Type for Graphics API/Method.
typedef enum box_renderer_backend_type {
    RENDERER_BACKEND_TYPE_VULKAN,
    RENDERER_BACKEND_TYPE_OPENGL,
    RENDERER_BACKEND_TYPE_DIRECTX
} box_renderer_backend_type;

// Type of GPU / Device.
typedef enum renderer_device_type {
    RENDERER_DEVICE_TYPE_OTHER = 0,
    RENDERER_DEVICE_TYPE_INTEGRATED_GPU = 1,
    RENDERER_DEVICE_TYPE_DISCRETE_GPU = 2,
    RENDERER_DEVICE_TYPE_VIRTUAL_GPU = 3,
    RENDERER_DEVICE_TYPE_CPU = 4,
} renderer_device_type;

// Modes for renderer to prepare for.
typedef enum renderer_mode {
    RENDERER_MODE_GRAPHICS = 1 << 0,
    RENDERER_MODE_COMPUTE = 1 << 1,
    RENDERER_MODE_TRANSFER = 1 << 2,
} renderer_mode;

typedef struct renderer_capabilities {
    // TODO: Remove - front end shouldn't know about these
    i32 graphics_queue_index;
    i32 present_queue_index;
    i32 transfer_queue_index;
    i32 compute_queue_index;

    char device_name[256];
    renderer_device_type device_type;
} renderer_capabilities;

// Configuration for render backend.
typedef struct renderer_backend_config {
    /// Enabled pipelines / modes for backend to prepare for.
    renderer_mode modes;

    // Send validation messages through out engine.
    b8 enable_validation;

    // Enable sampler anisotropy on GPU.
    b8 sampler_anisotropy;

    // Force renderer backend to use a discrete GPU.
    b8 discrete_gpu;

    // Chosen type of API. Keep this in renderer_backend_config so backend knows what type is it.
    box_renderer_backend_type api_type;

    // Frame count for swapchain, must be greater than 1.
    u32 swapchain_frame_count;

    // darray - Extensions enabled on graphics API. Consumed by renderer backend.
    const char** required_extensions;

    // NOTE: All variables below are exported from renderer backend.
    // 

    // Output of renderer backend.
    renderer_capabilities capabilities;

    // The framebuffer's current width.
    u32 framebuffer_width;

    // The framebuffer's current height.
    u32 framebuffer_height;
} box_renderer_backend_config;

// Sets default configurtion for renderer backend.
box_renderer_backend_config renderer_backend_default_config();

typedef struct box_renderer_backend {
    void* internal_context;
    struct box_platform* plat_state;

    b8 (*initialize)(struct box_renderer_backend* backend, uvec2 starting_size, const char* application_name, box_renderer_backend_config* config);

    void (*shutdown)(struct box_renderer_backend* backend);

    void (*wait_until_idle)(struct box_renderer_backend* backend, u64 timeout);
    void (*resized)(struct box_renderer_backend* backend, u32 width, u32 height);

    b8 (*begin_frame)(struct box_renderer_backend* backend, f32 delta_time);
    b8 (*playback_rendercmd)(struct box_renderer_backend* backend, struct box_rendercmd* rendercmd);
    b8 (*end_frame)(struct box_renderer_backend* backend);

    // Renderer resources.
    b8 (*create_internal_renderstage)(struct box_renderer_backend* backend, box_renderstage* out_stage);
    void (*destroy_internal_renderstage)(struct box_renderer_backend* backend, box_renderstage* out_stage);

    b8(*create_internal_renderbuffer)(struct box_renderer_backend* backend, box_renderbuffer* out_buffer);
    void (*destroy_internal_renderbuffer)(struct box_renderer_backend* backend, box_renderbuffer* out_buffer);
} box_renderer_backend;

b8 renderer_backend_create(box_renderer_backend_type type, struct box_platform* plat_state, box_renderer_backend* out_renderer_backend);
void renderer_backend_destroy(box_renderer_backend* renderer_backend);