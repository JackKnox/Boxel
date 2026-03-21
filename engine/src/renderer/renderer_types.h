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

#define BOX_FORMAT_UINT  0
#define BOX_FORMAT_SINT  1
#define BOX_FORMAT_FLOAT 2
#define BOX_FORMAT_BOOL  3

#define BOX_FORMAT_FLAG_NORMALIZED 1 << 0
#define BOX_FORMAT_FLAG_SRGB       1 << 1
//#define BOX_FORMAT_FLAG_DEPTH      = 1 << 2
//#define BOX_FORMAT_FLAG_STENCIL    = 1 << 3

#define BOX_RENDER_FORMAT(type, bits, channels, flags) \
    (((flags)              << 24)  | \
     ((type)               << 20)  | \
     (((bits / 8) - 1)     << 16)  | \
     ((channels)           << 12))

/**
 * @brief Describes a data format used by the renderer.
 *
 * Used for vertex attributes, textures, and general GPU data.
 */
typedef enum box_render_format {
    BOX_FORMAT_UNDEFINED = 0,

    /**
     * @brief 8-bit integer formats
     */
    BOX_FORMAT_R8_UINT    = BOX_RENDER_FORMAT(BOX_FORMAT_UINT, 8, 1, 0),
    BOX_FORMAT_RG8_UINT   = BOX_RENDER_FORMAT(BOX_FORMAT_UINT, 8, 2, 0),
    BOX_FORMAT_RGB8_UINT  = BOX_RENDER_FORMAT(BOX_FORMAT_UINT, 8, 3, 0),
    BOX_FORMAT_RGBA8_UINT = BOX_RENDER_FORMAT(BOX_FORMAT_UINT, 8, 4, 0),

    BOX_FORMAT_R8_SINT    = BOX_RENDER_FORMAT(BOX_FORMAT_SINT, 8, 1, 0),
    BOX_FORMAT_RG8_SINT   = BOX_RENDER_FORMAT(BOX_FORMAT_SINT, 8, 2, 0),
    BOX_FORMAT_RGB8_SINT  = BOX_RENDER_FORMAT(BOX_FORMAT_SINT, 8, 3, 0),
    BOX_FORMAT_RGBA8_SINT = BOX_RENDER_FORMAT(BOX_FORMAT_SINT, 8, 4, 0),

    /**
     * @brief 8-bit normalized formats
     */
    BOX_FORMAT_R8_UNORM    = BOX_RENDER_FORMAT(BOX_FORMAT_UINT, 8, 1, BOX_FORMAT_FLAG_NORMALIZED),
    BOX_FORMAT_RG8_UNORM   = BOX_RENDER_FORMAT(BOX_FORMAT_UINT, 8, 2, BOX_FORMAT_FLAG_NORMALIZED),
    BOX_FORMAT_RGB8_UNORM  = BOX_RENDER_FORMAT(BOX_FORMAT_UINT, 8, 3, BOX_FORMAT_FLAG_NORMALIZED),
    BOX_FORMAT_RGBA8_UNORM = BOX_RENDER_FORMAT(BOX_FORMAT_UINT, 8, 4, BOX_FORMAT_FLAG_NORMALIZED),

    BOX_FORMAT_R8_SNORM    = BOX_RENDER_FORMAT(BOX_FORMAT_SINT, 8, 1, BOX_FORMAT_FLAG_NORMALIZED),
    BOX_FORMAT_RG8_SNORM   = BOX_RENDER_FORMAT(BOX_FORMAT_SINT, 8, 2, BOX_FORMAT_FLAG_NORMALIZED),
    BOX_FORMAT_RGB8_SNORM  = BOX_RENDER_FORMAT(BOX_FORMAT_SINT, 8, 3, BOX_FORMAT_FLAG_NORMALIZED),
    BOX_FORMAT_RGBA8_SNORM = BOX_RENDER_FORMAT(BOX_FORMAT_SINT, 8, 4, BOX_FORMAT_FLAG_NORMALIZED),

    /**
     * @brief 16-bit formats
     */
    BOX_FORMAT_R16_UINT    = BOX_RENDER_FORMAT(BOX_FORMAT_UINT, 16, 1, 0),
    BOX_FORMAT_RG16_UINT   = BOX_RENDER_FORMAT(BOX_FORMAT_UINT, 16, 2, 0),
    BOX_FORMAT_RGB16_UINT  = BOX_RENDER_FORMAT(BOX_FORMAT_UINT, 16, 3, 0),
    BOX_FORMAT_RGBA16_UINT = BOX_RENDER_FORMAT(BOX_FORMAT_UINT, 16, 4, 0),

    BOX_FORMAT_R16_SINT    = BOX_RENDER_FORMAT(BOX_FORMAT_SINT, 16, 1, 0),
    BOX_FORMAT_RG16_SINT   = BOX_RENDER_FORMAT(BOX_FORMAT_SINT, 16, 2, 0),
    BOX_FORMAT_RGB16_SINT  = BOX_RENDER_FORMAT(BOX_FORMAT_SINT, 16, 3, 0),
    BOX_FORMAT_RGBA16_SINT = BOX_RENDER_FORMAT(BOX_FORMAT_SINT, 16, 4, 0),

    BOX_FORMAT_R16_FLOAT   = BOX_RENDER_FORMAT(BOX_FORMAT_FLOAT, 16, 1, 0),
    BOX_FORMAT_RG16_FLOAT  = BOX_RENDER_FORMAT(BOX_FORMAT_FLOAT, 16, 2, 0),
    BOX_FORMAT_RGB16_FLOAT = BOX_RENDER_FORMAT(BOX_FORMAT_FLOAT, 16, 3, 0),
    BOX_FORMAT_RGBA16_FLOAT = BOX_RENDER_FORMAT(BOX_FORMAT_FLOAT, 16, 4, 0),

    /**
     * @brief 32-bit formats
     */
    BOX_FORMAT_R32_UINT    = BOX_RENDER_FORMAT(BOX_FORMAT_UINT,  32, 1, 0),
    BOX_FORMAT_RG32_UINT   = BOX_RENDER_FORMAT(BOX_FORMAT_UINT,  32, 2, 0),
    BOX_FORMAT_RGB32_UINT  = BOX_RENDER_FORMAT(BOX_FORMAT_UINT,  32, 3, 0),
    BOX_FORMAT_RGBA32_UINT = BOX_RENDER_FORMAT(BOX_FORMAT_UINT,  32, 4, 0),

    BOX_FORMAT_R32_SINT    = BOX_RENDER_FORMAT(BOX_FORMAT_SINT,  32, 1, 0),
    BOX_FORMAT_RG32_SINT   = BOX_RENDER_FORMAT(BOX_FORMAT_SINT,  32, 2, 0),
    BOX_FORMAT_RGB32_SINT  = BOX_RENDER_FORMAT(BOX_FORMAT_SINT,  32, 3, 0),
    BOX_FORMAT_RGBA32_SINT = BOX_RENDER_FORMAT(BOX_FORMAT_SINT,  32, 4, 0),

    BOX_FORMAT_R32_FLOAT   = BOX_RENDER_FORMAT(BOX_FORMAT_FLOAT, 32, 1, 0),
    BOX_FORMAT_RG32_FLOAT  = BOX_RENDER_FORMAT(BOX_FORMAT_FLOAT, 32, 2, 0),
    BOX_FORMAT_RGB32_FLOAT = BOX_RENDER_FORMAT(BOX_FORMAT_FLOAT, 32, 3, 0),
    BOX_FORMAT_RGBA32_FLOAT = BOX_RENDER_FORMAT(BOX_FORMAT_FLOAT, 32, 4, 0),

    /**
     * @brief sRGB formats
     */
    BOX_FORMAT_R8_SRGB    = BOX_RENDER_FORMAT(BOX_FORMAT_UINT, 8, 1, BOX_FORMAT_FLAG_SRGB),
    BOX_FORMAT_RG8_SRGB   = BOX_RENDER_FORMAT(BOX_FORMAT_UINT, 8, 2, BOX_FORMAT_FLAG_SRGB),
    BOX_FORMAT_RGB8_SRGB  = BOX_RENDER_FORMAT(BOX_FORMAT_UINT, 8, 3, BOX_FORMAT_FLAG_SRGB),
    BOX_FORMAT_RGBA8_SRGB = BOX_RENDER_FORMAT(BOX_FORMAT_UINT, 8, 4, BOX_FORMAT_FLAG_SRGB),
} box_render_format;

/**
 * @brief Returns the size in bytes of a render format element.
 *
 * @param format Renderer description.
 * @return Size in bytes of a single element.
 */
u64 box_render_format_size(box_render_format format);

/**
 * @brief Returns if the render format is normalized.
 *
 * @param format Renderer description.
 * @return Whetever the passed render format is normalized.
 */
b8 box_render_format_normalized(box_render_format format);

/**
 * @brief Returns the channel count of a render format element.
 *
 * @param format Renderer description.
 * @return Channel count of a single element.
 */
u32 box_render_format_channel_count(box_render_format format);

/**
 * @brief Intended usage of a render buffer.
 *
 * Usage flags may be combined.
 */
typedef enum box_renderbuffer_usage {
    BOX_RENDERBUFFER_USAGE_VERTEX  = 1 << 0, /**< Vertex buffer */
    BOX_RENDERBUFFER_USAGE_INDEX   = 1 << 1, /**< Index buffer */
    BOX_RENDERBUFFER_USAGE_STORAGE = 1 << 2, /**< Storage buffer */
    BOX_RENDERBUFFER_USAGE_CPU_VISIBLE = 1 << 3, /**< CPU-coherent buffer */
} box_renderbuffer_usage;

/**
 * @brief Intended usage of a texture.
 *
 * Usage flags may be combined.
 */
typedef enum box_texture_usage {
    BOX_TEXTURE_USAGE_STORAGE = 1 << 0, /**< Storage image */
    BOX_TEXTURE_USAGE_SAMPLED = 1 << 1, /**< Texture created with sampler */
} box_texture_usage;

/**
 * @brief Type of render pass attachment.
 *
 * Defines the logical purpose of an attachment within a render pass.
 */
typedef enum box_attachment_type {
    BOX_ATTACHMENT_COLOR,   /**< Color render target (RGBA output) */
    BOX_ATTACHMENT_DEPTH,   /**< Depth-only attachment */
    BOX_ATTACHMENT_STENCIL, /**< Stencil-only attachment */
    BOX_ATTACHMENT_DEPTH_STENCIL, /**< Combined depth-stencil attachment */
} box_attachment_type;

/**
 * @brief Attachment load operation at the start of a render pass.
 *
 * Determines how existing attachment contents are treated
 * when the render pass begins.
 */
typedef enum box_load_op {
    BOX_LOAD_OP_LOAD,     /**< Preserve existing contents of the attachment */
    BOX_LOAD_OP_CLEAR,    /**< Clear the attachment at the start of the pass */
    BOX_LOAD_OP_DONT_CARE, /**< Contents are undefined at the start of the pass */
} box_load_op;

/**
 * @brief Attachment store operation at the end of a render pass.
 *
 * Determines whether attachment contents are preserved
 * after the render pass completes.
 */
typedef enum box_store_op {
    BOX_STORE_OP_STORE,     /**< Store the results for later use */
    BOX_STORE_OP_DONT_CARE, /**< Contents become undefined after the pass completes */
} box_store_op;

/**
 * @brief Describes memory access types used for synchronization between render stages.
 *
 * These flags represent how a resource is accessed within a render stage.
 * They are used to establish proper memory visibility and ordering guarantees 
 * between GPU operations.
 * 
 * Usage flags may be combined.
 */
typedef enum box_access_flags {
    BOX_ACCESS_FLAGS_SHADER_WRITE            = 1 << 0, /**< Written by a shader */
    BOX_ACCESS_FLAGS_SHADER_READ             = 1 << 1, /**< Resource read by a shader */
    BOX_ACCESS_FLAGS_VERTEX_ATTRIBUTE_READ   = 1 << 2, /**< Read as a vertex attribute during vertex input */
    BOX_ACCESS_FLAGS_COLOUR_ATTACHMENT_WRITE = 1 << 3  /**< Written as a colour attachment during rendering */
} box_access_flags;

/**
 * @brief Descriptor types supported by shaders.
 */
typedef enum box_descriptor_type {
    BOX_DESCRIPTOR_TYPE_STORAGE_BUFFER, /**< Storage buffer (SSBO) */
    BOX_DESCRIPTOR_TYPE_STORAGE_IMAGE,  /**< Storage image, no sampler */
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
 * @brief Describes a single render target attachment.
 */
typedef struct {
    /** Logical attachment type (color, depth, stencil, etc.). */
    box_attachment_type type;

    /** Engine-defined pixel format. Must be compatible with the attachment type. */
    box_render_format format;

    /** Load operation for color or depth aspect. */
    box_load_op load_op;

    /** Store operation for color or depth aspect. */
    box_store_op store_op;

    /**
     * Load operation for stencil aspect.
     * Only relevant for stencil or depth-stencil attachments.
     */
    box_load_op stencil_load_op;

    /**
     * Store operation for stencil aspect.
     * Only relevant for stencil or depth-stencil attachments.
     */
    box_store_op stencil_store_op;
} box_rendertarget_attachment;

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
 * @brief Raw shader stage data.
 *
 * Contains compiled bytecode loaded from disk or memory.
 */
typedef struct box_shader_src {
    /** @brief Pointer to compiled bytecode. */
    const void* data;

    /** @brief Size of the shader data in bytes. */
    u64 size;
} box_shader_src;

/**
 * @brief Describes the capabilities of the active renderer device.
 */
typedef struct {
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
    const char* application_name;

    uvec2 starting_size;

    box_rendertarget_attachment* main_attachments;
    u32 main_attachment_count;

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