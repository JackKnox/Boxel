#pragma once

#include "defines.h"

#include "utils/freelist.h"

// The long-term goal for the resource system is to become a general-purpose,
// engine-wide memory and lifecycle manager for global resources. It should:
//
// 1. Allocate and own storage for all engine-level resources.
// 2. Handle creation/destruction requests asynchronously on a dedicated thread.
// 3. Remove the need for any explicit type registry.
// 4. Allow each resource to define its own construction and destruction logic,
//    with the resource system acting only as the allocator + dispatcher.
//
// Ultimately this system should behave like:
//   - a thread-safe resource heap,
//   - a job dispatcher for resource operations,
//   - and a lifetime tracker for all engine resources.

// State of relevent resource through programs lifetime.
typedef enum box_resource_state {
	BOX_RESOURCE_STATE_UNINITIALIZED,
	BOX_RESOURCE_STATE_NEEDS_UPLOAD,  // Main thread requested upload.
	BOX_RESOURCE_STATE_UPLOADING,     // Render/upload thread is processing.
	BOX_RESOURCE_STATE_READY,         // GPU resource created and usable.
	BOX_RESOURCE_STATE_FAILED,        // Upload/create failed.
	BOX_RESOURCE_STATE_PENDING_DELETE // Flagged for deletion when safe.
} box_resource_state;

// Hold resource-specific function pointer to be used on resource creation thread.
typedef struct box_resource_vtable {
	b8 (*create_local)(struct box_resource_system* system, void* resource, void* args);
	void (*destroy_local)(struct box_resource_system* system, void* resource, void* args);
} box_resource_vtable;

// Start of every valid 'box_resource' in memory, holds private data when resource is fully initialized.
typedef struct box_resource_header {
	box_resource_vtable vtable;
	box_resource_state state;
	
	// Not managed by system, just held for benifit of user.
	void* resource_arg;
} box_resource_header;

typedef struct box_resource_system {
	freelist resources;
} box_resource_system;

b8 resource_system_init(box_resource_system* system, u64 start_mem);

b8 resource_system_new_resource(box_resource_system* system, void* resource, u64 resource_size, void* args);

void resource_thread_func(box_resource_system* system);

void resource_system_destroy_resources(box_resource_system* system);