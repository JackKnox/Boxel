/**
 * @file renderer_types.h
 * @brief Public renderer frontend types and configuration.
 *
 * Include this file to access renderer enums, formats,
 * configuration structures, and capability descriptions.
 */

#pragma once

#include "defines.h"

/**
 * @brief Supported renderer backend APIs.
 */
typedef enum box_renderer_backend_type {
    RENDERER_BACKEND_TYPE_VULKAN,   /**< Vulkan backend */
    RENDERER_BACKEND_TYPE_OPENGL,   /**< OpenGL backend */
    RENDERER_BACKEND_TYPE_DIRECTX,  /**< DirectX backend */
} box_renderer_backend_type;

/**
 * @brief Type of physical or logical rendering device.
 */
typedef enum box_renderer_device_type {
    RENDERER_DEVICE_TYPE_OTHER,           /**< Unknown or generic device */
    RENDERER_DEVICE_TYPE_INTEGRATED_GPU,  /**< Integrated GPU (shared memory) */
    RENDERER_DEVICE_TYPE_DISCRETE_GPU,    /**< Dedicated graphics card */
    RENDERER_DEVICE_TYPE_VIRTUAL_GPU,     /**< Virtualized GPU (e.g. VM) */
    RENDERER_DEVICE_TYPE_CPU,             /**< CPU-based rendering device */
} box_renderer_device_type;

/**
 * @brief Operating modes supported by the renderer.
 *
 * Modes may be combined as bit flags.
 */
typedef enum box_renderer_mode {
    RENDERER_MODE_GRAPHICS = 1 << 0,  /**< Graphics pipeline operations */
    RENDERER_MODE_COMPUTE  = 1 << 1,  /**< Compute shader operations */
    RENDERER_MODE_TRANSFER = 1 << 2,  /**< Data transfer operations */
} box_renderer_mode;

/**
 * @brief Base data types supported by the renderer.
 */
typedef enum box_format_type {
    BOX_FORMAT_TYPE_UINT8,    /**< Unsigned 8-bit integer */
    BOX_FORMAT_TYPE_UINT16,   /**< Unsigned 16-bit integer */
    BOX_FORMAT_TYPE_UINT32,   /**< Unsigned 32-bit integer */

    BOX_FORMAT_TYPE_SINT8,    /**< Signed 8-bit integer */
    BOX_FORMAT_TYPE_SINT16,   /**< Signed 16-bit integer */
    BOX_FORMAT_TYPE_SINT32,   /**< Signed 32-bit integer */

    BOX_FORMAT_TYPE_FLOAT32,  /**< 32-bit floating point */
    BOX_FORMAT_TYPE_SRGB,     /**< sRGB color format */

    BOX_FORMAT_TYPE_BOOL,     /**< Boolean type */
} box_format_type;

/**
 * @brief Describes a data format used by the renderer.
 *
 * Used for vertex attributes, textures, and general GPU data.
 */
typedef struct box_render_format {
    /** @brief Underlying data type. */
    box_format_type type;

    /** @brief Number of components per element (e.g., RGBA = 4). */
    u8 channel_count;

    /**
     * @brief Whether integer formats are normalized.
     *
     * If true, integer values are mapped to [0,1] or [-1,1]
     * when accessed in shaders.
     */
    b8 normalized;
} box_render_format;

/**
 * @brief Returns the size in bytes of a render format element.
 *
 * @param format Format description.
 * @return Size in bytes of a single element.
 */
u64 box_render_format_size(box_render_format format);

/**
 * @brief Supported primitive topologies.
 */
typedef enum box_vertex_topology_type {
    BOX_VERTEX_TOPOLOGY_TRIANGLES, /**< Triangle primitives */
    BOX_VERTEX_TOPOLOGY_POINTS,    /**< Point primitives */
    BOX_VERTEX_TOPOLOGY_LINES,     /**< Line primitives */
} box_vertex_topology_type;

/**
 * @brief Intended usage of a render buffer.
 *
 * Usage flags may be combined.
 */
typedef enum box_renderbuffer_usage {
    BOX_RENDERBUFFER_USAGE_VERTEX  = 1 << 0, /**< Vertex buffer */
    BOX_RENDERBUFFER_USAGE_INDEX   = 1 << 1, /**< Index buffer */
    BOX_RENDERBUFFER_USAGE_STORAGE = 1 << 2, /**< Storage buffer */
} box_renderbuffer_usage;

/**
 * @brief Descriptor types supported by shaders.
 */
typedef enum box_descriptor_type {
    BOX_DESCRIPTOR_TYPE_STORAGE_BUFFER, /**< Storage buffer (SSBO) */
    BOX_DESCRIPTOR_TYPE_IMAGE_SAMPLER,  /**< Combined image + sampler */
} box_descriptor_type;

/**
 * @brief Texture filtering modes.
 */
typedef enum box_filter_type {
    BOX_FILTER_TYPE_NEAREST, /**< Nearest-neighbor filtering */
    BOX_FILTER_TYPE_LINEAR,  /**< Linear interpolation filtering */
} box_filter_type;

/**
 * @brief Shader stage types.
 */
typedef enum box_shader_stage_type {
    BOX_SHADER_STAGE_TYPE_VERTEX,   /**< Vertex shader */
    BOX_SHADER_STAGE_TYPE_FRAGMENT, /**< Fragment/pixel shader */
    BOX_SHADER_STAGE_TYPE_COMPUTE,  /**< Compute shader */
    BOX_SHADER_STAGE_TYPE_MAX,      /**< Sentinel (max stages) */
} box_shader_stage_type;

/**
 * @brief Texture address (wrap) modes.
 */
typedef enum box_address_mode {
    BOX_ADDRESS_MODE_REPEAT,               /**< Repeat texture coordinates */
    BOX_ADDRESS_MODE_MIRRORED_REPEAT,      /**< Mirrored repeat */
    BOX_ADDRESS_MODE_CLAMP_TO_EDGE,        /**< Clamp to edge */
    BOX_ADDRESS_MODE_CLAMP_TO_BORDER,      /**< Clamp to border color */
    BOX_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE, /**< Mirror then clamp to edge */
} box_address_mode;

/**
 * @brief Describes a single descriptor binding.
 */
typedef struct {
    /** @brief Binding index within the shader. */
    u32 binding;

    /** @brief Descriptor resource type. */
    box_descriptor_type descriptor_type;

    /** @brief Shader stage this descriptor is visible to. */
    box_shader_stage_type stage_type;
} box_descriptor_desc;

/**
 * @brief Describes the capabilities of the active renderer device.
 */
typedef struct box_renderer_capabilities {
    /** @brief Human-readable device name. */
    char* device_name;

    /** @brief Device classification. */
    box_renderer_device_type device_type;

    /** @brief Maximum supported anisotropic filtering level. */
    f32 max_anisotropy;
} box_renderer_capabilities;

/**
 * @brief Configuration for creating a renderer backend.
 */
typedef struct box_renderer_backend_config {
    /** @brief Enabled renderer modes (bitmask). */
    box_renderer_mode modes;

    /** @brief Selected backend API type. */
    box_renderer_backend_type api_type;

    /**
     * @brief Number of frames processed concurrently.
     *
     * Must be greater than 1.
     */
    u32 frames_in_flight;

    /** @brief Enable validation and debug messages. */
    b8 enable_validation;

    /** @brief Enable sampler anisotropy if supported. */
    b8 sampler_anisotropy;

    /** @brief Prefer or require a discrete GPU. */
    b8 discrete_gpu;
} box_renderer_backend_config;

/**
 * @brief Returns a default-initialized renderer backend configuration.
 *
 * @return Default configuration values.
 */
box_renderer_backend_config box_renderer_backend_default_config();

/* Include public render object definitions */
#include "render_objects.h"