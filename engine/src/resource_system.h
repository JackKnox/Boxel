#pragma once

#include "defines.h"

#include "utils/freelist.h"

// State of valid resource through engine's lifetime.
typedef enum box_resource_state {
	BOX_RESOURCE_STATE_UNINITIALIZED,
	BOX_RESOURCE_STATE_NEEDS_UPLOAD,
	BOX_RESOURCE_STATE_UPLOADING,
	BOX_RESOURCE_STATE_READY,  
	BOX_RESOURCE_STATE_FAILED,
	BOX_RESOURCE_STATE_PENDING_DELETE // TODO: Might not need this?
} box_resource_state;

// Hold resource-specific function pointer to be used on resource creation thread.
typedef struct box_resource_vtable {
	b8 (*create_local)(struct box_resource_system* system, void* resource, void* args);
	void (*destroy_local)(struct box_resource_system* system, void* resource, void* args);
} box_resource_vtable;

// Start of every valid 'box_resource' in memory, holds private data when resource is fully initialized.
typedef struct box_resource_header {
	u8 magic;
	box_resource_vtable vtable;
	box_resource_state state;
	
	// Not managed by system, just held for benifit of user.
	void* resource_arg;
} box_resource_header;

// Opaque handle to true box_resource_system. TODO: Turn into private handle.
typedef struct box_resource_system {
	freelist resources;
	u64* upload_queue; // darray

	b8 is_running;
	u32 waiting_index;

	thrd_t resource_thread;
	mtx_t mutex;
	cnd_t cnd;
} box_resource_system;

// Creates and initializes the resource system, with allocating resource list with start_mem.
b8 resource_system_init(box_resource_system* system, u64 start_mem);

// Allocates memory for resource inline with internal data structures and returns address.
b8 resource_system_allocate_resource(box_resource_system* system, u64 resource_size, void** out_resource);

// After setting state for resource use this, to queue specified resource for creation.
void resource_system_signal_upload(box_resource_system* system, void* resource);

// Wait until all resources signaled for uploading before this function call.
void resource_system_wait(box_resource_system* system);

// Calls destruction function on all managed resources within specified system and deallocates memory.
void resource_system_destroy_resources(box_resource_system* system);