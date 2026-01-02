#pragma once

#include "defines.h"

// Type for Graphics API/Method.
typedef enum box_renderer_backend_type {
    RENDERER_BACKEND_TYPE_VULKAN,
    RENDERER_BACKEND_TYPE_OPENGL,
    RENDERER_BACKEND_TYPE_DIRECTX,
} box_renderer_backend_type;

// Type of GPU / Device.
typedef enum renderer_device_type {
    RENDERER_DEVICE_TYPE_OTHER,
    RENDERER_DEVICE_TYPE_INTEGRATED_GPU,
    RENDERER_DEVICE_TYPE_DISCRETE_GPU,
    RENDERER_DEVICE_TYPE_VIRTUAL_GPU,
    RENDERER_DEVICE_TYPE_CPU,
} renderer_device_type;

// Modes for renderer to prepare for.
typedef enum renderer_mode {
    RENDERER_MODE_GRAPHICS = 1 << 0,
    RENDERER_MODE_COMPUTE = 1 << 1,
    RENDERER_MODE_TRANSFER = 1 << 2,
} renderer_mode;

// Type of shader attached to a renderstage.
typedef enum box_shader_stage_type {
    BOX_SHADER_STAGE_TYPE_VERTEX,
    BOX_SHADER_STAGE_TYPE_GEOMETRY,
    BOX_SHADER_STAGE_TYPE_FRAGMENT,
    BOX_SHADER_STAGE_TYPE_COMPUTE,
    BOX_SHADER_STAGE_TYPE_MAX,
} box_shader_stage_type;

typedef enum box_filter_type {
    BOX_FILTER_TYPE_NEAREST,
    BOX_FILTER_TYPE_LINEAR,
} box_filter_type;

typedef enum box_address_mode {
    BOX_ADDRESS_MODE_REPEAT,
    BOX_ADDRESS_MODE_MIRRORED_REPEAT,
    BOX_ADDRESS_MODE_CLAMP_TO_EDGE,
    BOX_ADDRESS_MODE_CLAMP_TO_BORDER,
    BOX_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,
} box_address_mode;

typedef struct renderer_capabilities {
    const char* device_name;
    renderer_device_type device_type;
    f32 max_anisotropy;
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
    u32 frames_in_flight;

    // Output of renderer backend.
    renderer_capabilities capabilities;
} box_renderer_backend_config;

// Sets default configurtion for renderer backend.
box_renderer_backend_config renderer_backend_default_config();

typedef struct box_renderer_backend {
    void* internal_context;
    struct box_platform* plat_state;
    box_renderer_backend_config config;

    b8 (*initialize)(struct box_renderer_backend* backend, uvec2 starting_size, const char* application_name);

    void (*shutdown)(struct box_renderer_backend* backend);

    void (*wait_until_idle)(struct box_renderer_backend* backend, u64 timeout);
    void (*resized)(struct box_renderer_backend* backend, u32 width, u32 height);

    b8 (*begin_frame)(struct box_renderer_backend* backend, f32 delta_time);
    void (*playback_rendercmd)(struct box_renderer_backend* backend, struct box_rendercmd_context* rendercmd_context, struct rendercmd_header* header, union rendercmd_payload* payload);
    b8 (*end_frame)(struct box_renderer_backend* backend);

    // Renderer resources.
    b8 (*create_internal_renderstage)(struct box_renderer_backend* backend, struct box_renderstage* out_stage);
    void (*destroy_internal_renderstage)(struct box_renderer_backend* backend, struct box_renderstage* out_stage);

    b8(*create_internal_renderbuffer)(struct box_renderer_backend* backend, struct box_renderbuffer* out_buffer);
    b8(*upload_to_renderbuffer)(struct box_renderer_backend* backend, struct box_renderbuffer* buffer, void* data, u64 start_offset, u64 region);
    void (*destroy_internal_renderbuffer)(struct box_renderer_backend* backend, struct box_renderbuffer* buffer);

    b8(*create_internal_texture)(struct box_renderer_backend* backend, struct box_texture* out_texture);
    void (*destroy_internal_texture)(struct box_renderer_backend* backend, struct box_texture* texture);
} box_renderer_backend;

b8 renderer_backend_create(box_renderer_backend_type type, struct box_platform* plat_state, box_renderer_backend_config* config, box_renderer_backend* out_renderer_backend);
void renderer_backend_destroy(box_renderer_backend* renderer_backend);