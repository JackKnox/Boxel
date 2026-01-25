// Front end to renderer, to use ther renderer features just include this one file.
// Includes all basic structs / enums
#pragma once

#include "defines.h"

// Type of graphics API or backend being used.
typedef enum box_renderer_backend_type {
    RENDERER_BACKEND_TYPE_VULKAN,  // Vulkan backend
    RENDERER_BACKEND_TYPE_OPENGL,  // OpenGL backend
    RENDERER_BACKEND_TYPE_DIRECTX, // DirectX backend
} box_renderer_backend_type;

// Type of GPU / Device.
typedef enum box_renderer_device_type {
    RENDERER_DEVICE_TYPE_OTHER,            // Unknown or generic device
    RENDERER_DEVICE_TYPE_INTEGRATED_GPU,   // Integrated GPU (CPU + GPU on same chip)
    RENDERER_DEVICE_TYPE_DISCRETE_GPU,     // Discrete graphics card
    RENDERER_DEVICE_TYPE_VIRTUAL_GPU,      // Virtual GPU (e.g., in a VM)
    RENDERER_DEVICE_TYPE_CPU,              // CPU as rendering device
} box_renderer_device_type;

// Modes the renderer can operate in.
typedef enum box_renderer_mode {
    RENDERER_MODE_GRAPHICS = 1 << 0,   // Standard graphics rendering
    RENDERER_MODE_COMPUTE = 1 << 1,    // Compute operations (GPU compute shaders)
    RENDERER_MODE_TRANSFER = 1 << 2,   // Data transfer mode (e.g., buffer copies)
} box_renderer_mode;

// Supported format types for the renderer backend.
typedef enum box_format_type {
    BOX_FORMAT_TYPE_UINT8,   // Unsigned 8-bit integer
    BOX_FORMAT_TYPE_UINT16,  // Unsigned 16-bit integer
    BOX_FORMAT_TYPE_UINT32,  // Unsigned 32-bit integer

    BOX_FORMAT_TYPE_SINT8,   // Signed 8-bit integer
    BOX_FORMAT_TYPE_SINT16,  // Signed 16-bit integer
    BOX_FORMAT_TYPE_SINT32,  // Signed 32-bit integer

    BOX_FORMAT_TYPE_FLOAT32, // 32-bit floating point
    BOX_FORMAT_TYPE_SRGB,    // sRGB color format

    BOX_FORMAT_TYPE_BOOL,    // Boolean (true/false)
} box_format_type;

// Structure for format of general data in renderer backend.
typedef struct box_render_format {
    box_format_type type; // Data type
    u8 channel_count;     // Number of components per element (e.g., RGBA = 4)
    b8 normalized;        // Whether integer formats are normalized to [0,1] or [-1,1]
} box_render_format;

// Supported topologys for vertex data.
typedef enum box_vertex_topology_type {
    BOX_VERTEX_TOPOLOGY_TRIANGLES, // Triangles
    BOX_VERTEX_TOPOLOGY_POINTS,    // Points
    BOX_VERTEX_TOPOLOGY_LINES,     // Lines
} box_vertex_topology_type;

// Supported usages for a renderbuffer in host or local.
typedef enum box_renderbuffer_usage {
    BOX_RENDERBUFFER_USAGE_VERTEX  = 1 << 0, // Vertex buffer
    BOX_RENDERBUFFER_USAGE_INDEX   = 1 << 1, // Index buffer
    BOX_RENDERBUFFER_USAGE_STORAGE = 1 << 2, // Storage buffer for compute/other uses
} box_renderbuffer_usage;

// Types of descriptors or uniforms in the renderer.
typedef enum box_descriptor_type {
    BOX_DESCRIPTOR_TYPE_STORAGE_BUFFER, // Storage buffer (SSBO)
    BOX_DESCRIPTOR_TYPE_IMAGE_SAMPLER,  // Texture + sampler
} box_descriptor_type;

// Texture sampling filter types.
typedef enum box_filter_type {
    BOX_FILTER_TYPE_NEAREST, // Nearest-neighbor filtering
    BOX_FILTER_TYPE_LINEAR,  // Linear interpolation filtering
} box_filter_type;

// Type of shader attached to a renderstage.
typedef enum box_shader_stage_type {
    BOX_SHADER_STAGE_TYPE_VERTEX,   // Vertex shader stage
    BOX_SHADER_STAGE_TYPE_FRAGMENT, // Fragment/pixel shader stage
    BOX_SHADER_STAGE_TYPE_COMPUTE,  // Compute shader stage
    BOX_SHADER_STAGE_TYPE_MAX,      // Sentinel value (max number of shader stages)
} box_shader_stage_type;

// Texture address / wrap modes.
typedef enum box_address_mode {
    BOX_ADDRESS_MODE_REPEAT,               // Repeat texture coordinates
    BOX_ADDRESS_MODE_MIRRORED_REPEAT,      // Mirror and repeat
    BOX_ADDRESS_MODE_CLAMP_TO_EDGE,        // Clamp to edge pixels
    BOX_ADDRESS_MODE_CLAMP_TO_BORDER,      // Clamp to a border color
    BOX_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE, // Mirror then clamp to edge
} box_address_mode;

// Structure used to write descriptor values (buffers or textures) to GPU.
typedef struct box_write_descriptors {
    u32 binding;              // Binding point in shader
    const void* value;        // Pointer to buffer or texture
    box_descriptor_type type; // Type of descriptor
} box_write_descriptors;

// Structure describing the capabilities of a renderer/device.
typedef struct box_renderer_capabilities {
    char* device_name;                // Human-readable device name
    box_renderer_device_type device_type; // Device type (integrated, discrete, etc.)
    f32 max_anisotropy;               // Maximum supported anisotropic filtering
} box_renderer_capabilities;

// Configuration for render backend.
typedef struct box_renderer_backend_config {
    /// Enabled pipelines / modes for backend to prepare for.
    box_renderer_mode modes;

    // Send validation messages through out engine.
    b8 enable_validation;

    // Enable sampler anisotropy on GPU.
    b8 sampler_anisotropy;

    // Force renderer backend to use a discrete GPU.
    b8 discrete_gpu;

    // Chosen type of API. Keep this in box_renderer_backend_config so backend knows what type is it.
    box_renderer_backend_type api_type;

    // Frame count for swapchain, must be greater than 1.
    u32 frames_in_flight;
} box_renderer_backend_config;

// Sets default configurtion for renderer backend.
box_renderer_backend_config renderer_backend_default_config();

#include "render_layout.h"
#include "render_objects.h"