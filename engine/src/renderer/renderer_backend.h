#pragma once

#include "defines.h"

#include "platform/platform.h"
#include "renderer/renderer_types.h"

/**
 * @brief Render command types.
 *
 * Represent high-level rendering operations that are translated
 * into backend-specific API calls (OpenGL, Vulkan, etc.).
 */
enum {
    RENDERCMD_BIND_RENDERTARGET,
    RENDERCMD_MEMORY_BARRIER,
    RENDERCMD_BEGIN_RENDERSTAGE,
    RENDERCMD_END_RENDERSTAGE,
    RENDERCMD_DRAW,
    RENDERCMD_DRAW_INDEXED,
    RENDERCMD_DISPATCH,

    /** @brief Internal command used to finalize command buffers. */
    RENDERCMD_END,
};

/** @brief Type used to identify a render command payload. */
typedef u32 rendercmd_payload_type;

#pragma pack(push, 1)

/**
 * @brief Header prepended to every render command payload.
 *
 * Describes the command type and the renderer mode it supports.
 */
typedef struct rendercmd_header {
    /** @brief Type of render command. */
    rendercmd_payload_type type;

    /** @brief Renderer mode this command is valid for (graphics or compute). */
    box_renderer_mode supported_mode;
} rendercmd_header;

#pragma pack(pop)

#pragma pack(push, 1)

/**
 * @brief Payload data for all supported render commands.
 *
 * The active member is determined by rendercmd_payload_type.
 */
typedef union rendercmd_payload {
    /**
     * @brief Clear color command payload.
     */
    struct {
        /** @brief Render target to bind for subsequent operations. */
        box_rendertarget* rendertarget;
    } bind_rendertarget;

    struct {
        box_renderstage* src_renderstage, * dst_renderstage;
        box_access_flags src_access, dst_access;
    } memory_barrier;

    /**
     * @brief Begin render stage command payload.
     */
    struct {
        /** @brief Render stage to bind for subsequent operations. */
        box_renderstage* renderstage;
    } begin_renderstage;

    /**
     * @brief Non-indexed draw command payload.
     */
    struct {
        /** @brief Number of vertices to draw. */
        u32 vertex_count;

        /** @brief Number of instances to draw. */
        u32 instance_count;
    } draw;

    /**
     * @brief Indexed draw command payload.
     */
    struct {
        /** @brief Number of indices to draw. */
        u32 index_count;

        /** @brief Number of instances to draw. */
        u32 instance_count;
    } draw_indexed;

    /**
     * @brief Compute dispatch command payload.
     */
    struct {
        /** @brief Compute workgroup dimensions. */
        uvec3 group_size;
    } dispatch;

} rendercmd_payload;

#pragma pack(pop)

/**
 * @brief Context used during render command playback.
 *
 * Tracks currently active render state while commands are executed.
 */
typedef struct box_rendercmd_context {
    /** @brief Current renderer mode (graphics or compute). */
    box_renderer_mode current_mode;

    /** @brief Currently bound render target. */
    box_rendertarget* current_target;

    /** @brief Currently bound render stage. */
    box_renderstage* current_shader;
} box_rendercmd_context;

/**
 * @brief Renderer backend interface.
 *
 * Provides a backend-agnostic abstraction over graphics APIs.
 * Implementations translate engine commands into API-specific calls.
 */
typedef struct box_renderer_backend {
    /** @brief Backend-specific internal state. */
    void* internal_context;

    /** @brief Platform state attachted to renderer backend, can be null */
    box_platform* platform;

    /** @brief Renderer capabilities supported by this backend. */
    box_renderer_capabilities capabilities;

    /** @brief Main rendertarget created automattically on init, exists regardless of platform state */
    box_rendertarget main_rendertarget;

    /**
     * @brief Initializes the renderer backend.
     *
     * @param backend Pointer to the backend instance.
     * @param config Backend configuration.
     * @param starting_size Initial framebuffer size.
     * @param application_name Name of the application.
     * @return True if initialization succeeded.
     */
    b8 (*initialize)(struct box_renderer_backend* backend, box_renderer_backend_config* config);

    /**
     * @brief Shuts down the renderer backend and releases resources.
     */
    void (*shutdown)(struct box_renderer_backend* backend);

    /**
     * @brief Notifies the backend of a framebuffer resize.
     *
     * @param backend Pointer to the backend instance.
     * @param new_size New framebuffer dimensions.
     */
    void (*resized)(struct box_renderer_backend* backend, uvec2 new_size);

    /**
     * @brief Begins a new frame.
     *
     * @param backend Pointer to the backend instance.
     * @param delta_time Time elapsed since last frame.
     * @return True if frame preparation succeeded.
     */
    b8 (*begin_frame)(struct box_renderer_backend* backend, f64 delta_time);

    /**
     * @brief Executes a single render command.
     *
     * @param backend Pointer to the backend instance.
     * @param playback_context Playback state context.
     * @param header Command header.
     * @param payload Command payload.
     */
    void (*execute_command)(struct box_renderer_backend* backend, box_rendercmd_context* playback_context, rendercmd_header* header, rendercmd_payload* payload);

    /**
     * @brief Ends the current frame.
     *
     * @return True if frame submission succeeded.
     */
    b8 (*end_frame)(struct box_renderer_backend* backend);

    /** @name Resource Management */
    /** @{ */

    /** @brief Creates a render stage with graphics configuration. */
    b8 (*create_graphicstage)(struct box_renderer_backend* backend, box_graphicstage_config* config, box_rendertarget* bound_rendertarget, box_renderstage* out_graphicstage);

    /** @brief Creates a render stage with compute configuration. */
    b8 (*create_computestage)(struct box_renderer_backend* backend, box_computestage_config* config, box_renderstage* out_computestage);

    /**
     * @brief Updates one or more descriptor bindings for a renderstage.
     *
     * @param backend           Pointer to the renderer backend.
     * @param descriptors       Array of descriptor update descriptions.
     * @param descriptor_count  Number of elements in @p descriptors.
     */
    b8 (*update_renderstage_descriptors) (struct box_renderer_backend* backend, box_update_descriptors* descriptors, u32 descriptor_count);

    /** @brief Destroys a render stage. */
    void (*destroy_renderstage)(struct box_renderer_backend* backend, box_renderstage* stage);
    
    /** @brief Creates a render buffer. */
    b8 (*create_renderbuffer)(struct box_renderer_backend* backend, box_renderbuffer_config* config, box_renderbuffer* out_buffer);

    /**
     * @brief Uploads data into a render buffer.
     *
     * @param backend Pointer to backend.
     * @param buffer Target buffer.
     * @param data Source data pointer.
     * @param start_offset Offset into buffer.
     * @param region Size of data in bytes.
     */
    b8 (*upload_to_renderbuffer)(struct box_renderer_backend* backend, box_renderbuffer* buffer, const void* data, u64 start_offset, u64 region);

    /** @brief Destroys a render buffer. */
    void (*destroy_renderbuffer)(struct box_renderer_backend* backend, box_renderbuffer* buffer);

    /** @brief Creates a texture resource. */
    b8 (*create_texture)(struct box_renderer_backend* backend, box_texture_config* config, box_texture* out_texture);

    /**
     * @brief Uploads data into a texture.
     *
     * @param backend Pointer to backend.
     * @param buffer Target texture.
     * @param data Source data pointer.
     * @param start_offset Offset into image.
     * @param region Size of data in width & height.
     */
    b8 (*upload_to_texture)(struct box_renderer_backend* backend, box_texture* texture, const void* data, uvec2 start_offset, uvec2 region);

    /** @brief Destroys a texture resource. */
    void (*destroy_texture)(struct box_renderer_backend* backend, box_texture* texture);

    /** @brief Creates a rendertarget. */
    b8 (*create_rendertarget)(struct box_renderer_backend* backend,  box_rendertarget_config* config, box_rendertarget* rendertarget);

    /** @brief Destroys a rendertarget. */
    void (*destroy_rendertarget)(struct box_renderer_backend* backend, box_rendertarget* rendertarget);

    /** @} */
} box_renderer_backend;

/**
 * @brief Creates and initializes a renderer backend.
 *
 * @param renderer_backend Output backend instance.
 * @param config Backend configuration.
 * @return True if creation succeeded.
 */
b8 box_renderer_backend_create(box_renderer_backend* renderer_backend, box_renderer_backend_config* config, box_platform* platform);

/**
 * @brief Destroys a renderer backend instance.
 *
 * @param renderer_backend Backend to destroy.
 */
void box_renderer_backend_destroy(box_renderer_backend* renderer_backend);

/**
 * @brief Executes a sequence of render command buffers.
 *
 * @param renderer_backend Backend instance.
 * @param playback_context Playback state context.
 * @param rendercmd Command buffer to execute.
 * @return True if execution succeeded.
 */
b8 box_renderer_backend_submit_rendercmd(box_renderer_backend* renderer_backend, box_rendercmd_context* playback_context, box_rendercmd* rendercmd);