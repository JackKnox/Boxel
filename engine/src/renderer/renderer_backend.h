// Private stuff to renderer backend
// engine_private.h should include this to store rendeer backend in struct.
#pragma once

#include "defines.h"

#include "renderer/renderer_types.h"

// These describe high-level rendering intent and are translated into backend-specific API calls (OpenGL, Vulkan, etc).
enum {
    RENDERCMD_SET_CLEAR_COLOUR,
    RENDERCMD_BEGIN_RENDERSTAGE,
    RENDERCMD_END_RENDERSTAGE,
    RENDERCMD_DRAW,
    RENDERCMD_DRAW_INDEXED,
    RENDERCMD_DISPATCH,

    // Private command used internally by the renderer backend to finalize state.
    _RENDERCMD_END,
};
typedef u32 rendercmd_payload_type;

#pragma pack(push, 1)
// Header prepended to every render command payload.
typedef struct rendercmd_header {
    // Type of render command.
    rendercmd_payload_type type;

    // Renderer mode this command is valid for (graphics / compute).
    box_renderer_mode supported_mode;
} rendercmd_header;
#pragma pack(pop)

#pragma pack(push, 1)
// Payload data for all supported render commands.
// Interpreted based on rendercmd_payload_type.
typedef union rendercmd_payload {
    struct {
        // Packed RGBA clear colour.
        u32 clear_colour;
    } set_clear_colour;

    struct {
        // Renderstage to bind for subsequent draw or dispatch calls.
        box_renderstage* renderstage;
    } begin_renderstage;

    struct {
        // Number of vertices to draw.
        u32 vertex_count;

        // Number of instances to draw.
        u32 instance_count;
    } draw;

    struct {
        // Number of indices to draw.
        u32 index_count;

        // Number of instances to draw.
        u32 instance_count;
    } draw_indexed;

    struct {
        // Compute workgroup dimensions.
        uvec3 group_size;
    } dispatch;

} rendercmd_payload;
#pragma pack(pop)

// Context for issuing render commands.
typedef struct rendercmd_context {
    box_renderstage* current_shader; // Currently bound render stage / shader
    box_renderer_mode current_mode;  // Current mode (graphics, compute)
} rendercmd_context;

typedef struct box_renderer_backend {
    void* internal_context;
    struct box_platform* plat_state;
    box_renderer_capabilities capabilities;

    b8 (*initialize)(struct box_renderer_backend* backend, box_renderer_backend_config* config, uvec2 starting_size, const char* application_name);

    void (*shutdown)(struct box_renderer_backend* backend);

    void (*wait_until_idle)(struct box_renderer_backend* backend, u64 timeout);
    void (*resized)(struct box_renderer_backend* backend, uvec2 new_size);

    b8 (*begin_frame)(struct box_renderer_backend* backend, f32 delta_time);
    void (*playback_rendercmd)(struct box_renderer_backend* backend, rendercmd_context* rendercmd_context, rendercmd_header* header, rendercmd_payload* payload);
    b8 (*end_frame)(struct box_renderer_backend* backend);

    // Renderer resources.
    b8 (*create_internal_renderstage)(struct box_renderer_backend* backend, box_renderstage* out_stage);
    b8 (*write_renderstage_descriptors)(struct box_renderer_backend* backend, box_renderstage* stage, box_write_descriptors* writes, u64 write_count);
    void (*destroy_internal_renderstage)(struct box_renderer_backend* backend, box_renderstage* stage);
    
    b8 (*create_internal_renderbuffer)(struct box_renderer_backend* backend, box_renderbuffer* out_buffer);
    b8 (*upload_to_renderbuffer)(struct box_renderer_backend* backend, box_renderbuffer* buffer, void* data, u64 start_offset, u64 region);
    void (*destroy_internal_renderbuffer)(struct box_renderer_backend* backend, box_renderbuffer* buffer);

    b8 (*create_internal_texture)(struct box_renderer_backend* backend, box_texture* out_texture);
    void (*destroy_internal_texture)(struct box_renderer_backend* backend, box_texture* texture);
} box_renderer_backend;

b8 box_renderer_backend_create(box_renderer_backend_config* config, uvec2 starting_size, const char* application_name, struct box_platform* plat_state, box_renderer_backend* out_renderer_backend);
void box_renderer_backend_destroy(box_renderer_backend* renderer_backend);