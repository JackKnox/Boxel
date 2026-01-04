#pragma once

#include "defines.h"

#include "utils/freelist.h"

#include "core/job_system.h"

// Represents the lifecycle state of a resource throughout the resource system's runtime.
typedef enum {
	// Resource has been created but no data has been assigned or loaded.
	BOX_RESOURCE_STATE_UNINITIALIZED,

	// Resource data is available on the CPU and must be uploaded to the GPU.
	BOX_RESOURCE_STATE_NEEDS_UPLOAD,

	// Resource is currently being uploaded (e.g. async transfer in progress).
	BOX_RESOURCE_STATE_UPLOADING,

	// Resource is fully initialized and ready for use by the renderer.
	BOX_RESOURCE_STATE_READY,

	// Resource failed to load or upload and is not usable.
	BOX_RESOURCE_STATE_FAILED,
} box_resource_state;

// Holds resource-specific function pointers executed on the resource creation thread.
typedef struct {
	// Creates or initializes the resource in a thread-local context.
	b8 (*create_local)(struct box_resource_system* system, void* resource, void* args);

	// Destroys or cleans up the resource in a thread-local context.
	void (*destroy_local)(struct box_resource_system* system, void* resource, void* args);
} box_resource_vtable;

// Header present at the start of every valid box_resource in memory.
typedef struct {
	// Resource-specific callbacks used during creation and destruction.
	box_resource_vtable vtable;

	// User-owned argument passed to resource callbacks.
	void* resource_arg;

	// Current lifecycle state of the resource.
	box_resource_state state;

	// Magic value used to validate resource memory and detect corruption or invalid casts during development and debugging.
	u8 magic;
} box_resource_header;

// Opaque handle to true box_resource_system.
typedef struct box_resource_system {
	freelist resources;
	job_worker worker;
} box_resource_system;

// Creates and initializes the resource system, allocate internal resource storage with size of start_mem.
b8 box_resource_system_init(box_resource_system* system, u64* memory_usage, u64 start_mem);

// Allocates memory for a resource, including its internal header and bookkeeping data.
void* box_resource_system_allocate_resource(box_resource_system* system, u64 resource_size);

// Queues a resource for creation/upload after its state has been set appropriately.
void box_resource_system_signal_upload(box_resource_system* system, void* resource);

// Blocks until all previously signaled resource uploads have completed.
void box_resource_system_flush_uploads(box_resource_system* system);

// Destroys all managed resources and releases all system-owned memory.
void box_resource_system_shutdown(box_resource_system* system);