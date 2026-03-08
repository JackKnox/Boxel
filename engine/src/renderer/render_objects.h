/**
 * @file renderer_objects.h
 * @brief Backend-agnostic GPU resource and render command abstractions.
 *
 * Defines core rendering resources such as buffers, textures,
 * render stages (pipelines), and command buffers.
 */

#pragma once

#include "defines.h"
#include "utils/freelist.h"

/**
 * @brief Backend-agnostic GPU buffer handle.
 *
 * Represents a GPU-resident buffer resource such as a vertex,
 * index, uniform, or storage buffer.
 */
typedef struct box_renderbuffer {
    /** @brief Total size of the buffer in bytes. */
    u64 buffer_size;

    /** @brief Backend-specific buffer state/handle. */
    void* internal_data;

    /** @brief Intended usage of the buffer (vertex, index, uniform, etc.). */
    box_renderbuffer_usage usage;
} box_renderbuffer;

/**
 * @brief Creates a default-initialized render buffer.
 *
 * @return A zeroed/default render buffer instance.
 */
box_renderbuffer box_renderbuffer_default();

/**
 * @brief Raw shader stage data.
 *
 * Contains shader source code or compiled bytecode
 * loaded from disk or memory.
 */
typedef struct {
    /** @brief Pointer to shader source or compiled bytecode. */
    const void* file_data;

    /** @brief Size of the shader data in bytes. */
    u64 file_size;
} shader_stage;

#define BOX_MAX_VERTEX_ATTRIBS 8
#define BOX_MAX_DESCRIPTORS 8

/**
 * @brief Represents a render pipeline configuration.
 *
 * A render stage defines shader stages, vertex layout,
 * descriptor bindings, and pipeline state.
 *
 * This structure is backend-agnostic and translated
 * into API-specific pipeline objects internally.
 */
typedef struct box_renderstage {
    /** @brief Shader stages indexed by box_shader_stage_type. */
    shader_stage stages[BOX_SHADER_STAGE_TYPE_MAX];
    
	/** @brief Descriptor binding descriptions used by the pipeline. */
	box_descriptor_desc descriptors[BOX_MAX_DESCRIPTORS];

    /** @brief Number of active descriptor bindings. */
	u8 descriptor_count;

    /** @brief Rendering mode (graphics, compute, etc.). */
    box_renderer_mode mode;

    union {
        /**
         * @brief Graphics pipeline configuration.
         */
        struct {
            /** @brief Vertex attribute formats in binding order. */
            box_render_format vertex_attributes[BOX_MAX_VERTEX_ATTRIBS];

            /** @brief Number of active vertex attributes. */
            u8 vertex_attribute_count;

            /** @brief Optional vertex buffer bound to this render stage. */
            box_renderbuffer* vertex_buffer;

            /** @brief Optional index buffer bound to this render stage. */
            box_renderbuffer* index_buffer;
        } graphics;

        /**
         * @brief Compute pipeline configuration.
         */
        struct {
            /* Future compute-specific configuration */
        } compute;

    };

    /** @brief Backend-specific pipeline or program state. */
    void* internal_data;
} box_renderstage;

/**
 * @brief Creates a default render stage with graphics configuration.
 *
 * @param shader_stages Array of shader file paths or stage identifiers.
 * @param shader_stage_count Number of shader stages provided.
 *
 * @return A default-initialized graphics render stage.
 */
box_renderstage box_renderstage_graphics_default(const char** shader_stages, u8 shader_stage_count);

box_renderstage box_renderstage_compute_default(const char** shader_stages, u8 shader_stage_count);

/**
 * @brief Backend-agnostic texture resource.
 *
 * Represents GPU image data along with sampling configuration.
 * Converted internally into API-specific image and sampler objects.
 */
typedef struct box_texture {
    /** @brief Dimensions of the texture in pixels. */
    uvec2 size;
       
    /** @brief Image format of the texture data. */
    box_render_format image_format;

    /** @brief Backend-specific image and sampler state. */
    void* internal_data;

    /** @brief Texture filtering mode. */
    box_filter_type filter_type;

    /** @brief Texture address (wrap) mode. */
    box_address_mode address_mode;

    /** @brief Max anisotropy of attached sampler in backend, ignored if texture is not sampled. */
    f32 max_anisotropy;

    box_texture_usage usage;
} box_texture;

/**
 * @brief Creates a default-initialized texture configuration.
 *
 * @return A zeroed/default texture instance.
 */
box_texture box_texture_default();

/**
 * @brief Calculates the total memory size of a texture in bytes.
 *
 * @param texture Pointer to a valid box_texture instance.
 *
 * @return The total size of the texture in bytes.
 */
u64 box_texture_get_size_in_bytes(box_texture* texture);

/**
 * @brief Describes a single render target attachment.
 */
typedef struct box_rendertarget_attachment {
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
 * @brief Represents a render target used by the renderer backend.
 *
 * A render target is the destination for rendering operations. It can either
 * represent a window-backed surface (such as a swapchain image) or an
 * offscreen render target (such as a framebuffer or texture).
 */
typedef struct box_rendertarget {
    /** @brief Clear colour value used when beginning a render pass. */
    u32 clear_colour;

    /** @brief Render area origin. */
    uvec2 origin;

    /** @brief Render area size. */
    uvec2 size;

    box_rendertarget_attachment* attachments;

    u32 attachment_count;

    /** @brief Backend-specific internal data. */
    void* internal_data;
} box_rendertarget;

box_rendertarget box_rendertarget_default();

/**
 * @brief Describes a single descriptor update for a renderstage.
 *
 * This structure is used to update a descriptor binding within a
 * box_renderstage. The descriptor type determines which union member
 * is expected to be valid.
 *
 * @note Only one union member must be set, according to the value of @ref type.
 */
typedef struct {
    /** @brief Renderstage to update descriptor to. */
    box_renderstage* renderstage;

    /** @brief Binding index as declared in the shader. */
    u32 binding;

    /** @brief Type of descriptor being updated (buffer, texture, etc.). */
    box_descriptor_type type;

    union {
        /** @brief Render buffer resource (uniform or storage). */
        box_renderbuffer* buffer;

        /** @brief Texture resource (sampled or storage texture). */
        box_texture* texture;
    };
} box_update_descriptors;

/**
 * @brief Render command buffer.
 *
 * Collects rendering commands for deferred execution
 * by the renderer.
 */
typedef struct {
    /** @brief Allocator backing the render command buffer. */
    freelist buffer;

    /** @brief True if no further commands may be written. */
    b8 finished;
} box_rendercmd;

/**
 * @brief Resets the render command buffer without reallocating memory.
 *
 * Clears all recorded commands but preserves allocated capacity.
 *
 * @param cmd Pointer to the command buffer.
 */
void box_rendercmd_begin(box_rendercmd* cmd);

/**
 * @brief Destroys and frees memory associated with the command buffer.
 *
 * @param cmd Pointer to the command buffer.
 */
void box_rendercmd_destroy(box_rendercmd* cmd);

/**
 * @brief Binds a render target to a render command.
 *
 * This function sets the specified render target as the current target
 * for subsequent rendering operations associated with the given render command.
 *
 * @param cmd Pointer to the command buffer.
 * @param rendertarget Pointer to the render target to bind.
 */
void box_rendercmd_bind_rendertarget(box_rendercmd* cmd, box_rendertarget* rendertarget);

/**
 * @brief Inserts a GPU memory barrier between two render stages.
 *
 * Ensures that memory writes performed in the source render stage
 * become visible to subsequent memory accesses in the destination
 * render stage.
 * 
 * @param cmd Pointer to the command buffer.
 * @param src_renderstage The render stage that performed the writes.
 * @param dst_renderstage The render stage that will perform subsequent reads/writes.
 * @param src_access The type of access performed in the source stage (typically write flags).
 * @param dst_access The type of access that will occur in the destination stage.
 */
void box_rendercmd_memory_barrier(box_rendercmd* cmd, box_renderstage* src_renderstage, box_renderstage* dst_renderstage, box_access_flags src_access, box_access_flags dst_access);

/**
 * @brief Begins a render stage.
 *
 * Subsequent draw or dispatch calls will use this stage.
 *
 * @param cmd Pointer to the command buffer.
 * @param renderstage Pointer to the render stage to bind.
 */
void box_rendercmd_begin_renderstage(box_rendercmd* cmd, box_renderstage* renderstage);

/**
 * @brief Issues a non-indexed draw call.
 *
 * @param cmd Pointer to the command buffer.
 * @param vertex_count Number of vertices to draw.
 * @param instance_count Number of instances to render.
 */
void box_rendercmd_draw(box_rendercmd* cmd, u32 vertex_count, u32 instance_count);

/**
 * @brief Issues an indexed draw call.
 *
 * @param cmd Pointer to the command buffer.
 * @param index_count Number of indices to draw.
 * @param instance_count Number of instances to render.
 */
void box_rendercmd_draw_indexed(box_rendercmd* cmd, u32 index_count, u32 instance_count);

/**
 * @brief Dispatches a compute workload.
 *
 * @param cmd Pointer to the command buffer.
 * @param group_size_x Number of workgroups in X.
 * @param group_size_y Number of workgroups in Y.
 * @param group_size_z Number of workgroups in Z.
 */
void box_rendercmd_dispatch(box_rendercmd* cmd, u32 group_size_x, u32 group_size_y, u32 group_size_z);

/**
 * @brief Ends the current render stage.
 *
 * Finalizes state for the active renderstage.
 *
 * @param cmd Pointer to the command buffer.
 */
void box_rendercmd_end_renderstage(box_rendercmd* cmd);

/**
 * @brief Finalizes the command buffer.
 *
 * Prevents further modification and marks it ready
 * for consumption by the renderer backend.
 *
 * @param cmd Pointer to the command buffer.
 */
void box_rendercmd_end(box_rendercmd* cmd);
