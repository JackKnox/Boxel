#include "defines.h"
#include "resource_system.h"

#include "engine.h"
#include "utils/darray.h"

// Represents the resource management system of the engine.
typedef struct box_resource_system {
    // Dynamic array of resource headers owned by this system - darray.
    box_resource_header** owned_resources;

    // Job worker used to process resource tasks asynchronously.
    job_worker worker;
} box_resource_system;

#define RESOURCE_MAGIC 0x52

b8 resource_thread_func(job_worker* worker, void* job, box_resource_system* system /* void* arg */) {
    BX_ASSERT(worker != NULL && job != NULL && system != NULL && "Invalid arguments passed to resource_thread_func");
    box_resource_header* resource = *(box_resource_header**)job;

    if (resource->magic != RESOURCE_MAGIC) {
        BX_WARN("Resource in upload queue had invalid magic; skipping.");
        return FALSE;
    }

    if (resource->state != BOX_RESOURCE_STATE_NEEDS_UPLOAD)
        return FALSE;

    resource->state = BOX_RESOURCE_STATE_UPLOADING;

    // Perform the create (does its own GPU/local allocations)
    b8 created = resource->vtable.create_local(system, resource, resource->resource_arg);
    resource->state = created ? BOX_RESOURCE_STATE_READY : BOX_RESOURCE_STATE_FAILED;
    return created;
}

b8 resource_system_init(box_resource_system* system, burst_allocator* allocator, box_resource_system** out_block) {
    if (allocator != NULL) {
        burst_add_block(allocator, sizeof(box_resource_system), MEMORY_TAG_RESOURCES, out_block);
        return TRUE;
    }

    BX_ASSERT(system != NULL && "Invalid arguments passed to resource_system_init");

    system->owned_resources = darray_create(box_resource_header*, MEMORY_TAG_RESOURCES);

    if (!job_worker_start(&system->worker, resource_thread_func, sizeof(box_resource_header*), MEMORY_TAG_RESOURCES, (void*)system)) {
        BX_ERROR("Could not start resource thread");
        return FALSE;
    }

    return TRUE;
}

void* resource_system_allocate_resource(box_resource_system* system, u64 size) {
    BX_ASSERT(system != NULL && size > 0 && "Invalid arguments passed to resource_system_allocate_resource");
    box_resource_header* new_resource = ballocate(size, MEMORY_TAG_RESOURCES);
    new_resource->magic = RESOURCE_MAGIC;
    new_resource->allocation_size = size;
    new_resource->state = BOX_RESOURCE_STATE_UNINITIALIZED;

    system->owned_resources = _darray_push(system->owned_resources, &new_resource);
    return new_resource;
}

void resource_system_signal_upload(box_resource_system* system, void* resource) {
    BX_ASSERT(system != NULL && resource != NULL && "Invalid arguments passed to resource_system_signal_upload");
    box_resource_header* hdr = (box_resource_header*)resource;
    if (hdr->magic != RESOURCE_MAGIC) {
        BX_WARN("Invalid resource passed to upload.");
        return;
    }

    if (hdr->state != BOX_RESOURCE_STATE_UNINITIALIZED) {
        BX_WARN("Resource signalled for upload more than once");
        return;
    }

    hdr->state = BOX_RESOURCE_STATE_NEEDS_UPLOAD;
    job_worker_push(&system->worker, &resource);
}

void resource_system_flush_uploads(box_resource_system* system) {
    BX_ASSERT(system != NULL && "Invalid arguments passed to resource_system_flush_uploads");
    job_worker_wait_until_idle(&system->worker);
}

void resource_system_shutdown(box_resource_system* system) {
    BX_ASSERT(system != NULL && "Invalid arguments passed to resource_system_shutdown");
    job_worker_wait_until_idle(&system->worker);
    job_worker_stop(&system->worker);

    for (u32 i = 0; i < darray_length(system->owned_resources); ++i) {
        box_resource_header* hdr = system->owned_resources[i];
        if (hdr->magic != RESOURCE_MAGIC) continue;
        BX_ASSERT(hdr->state != BOX_RESOURCE_STATE_UNINITIALIZED && "Allocated resource within system but didn't signal upload.");

        if ((hdr->state == BOX_RESOURCE_STATE_READY || hdr->state == BOX_RESOURCE_STATE_FAILED) && hdr->vtable.destroy_local) {
            hdr->vtable.destroy_local(system, hdr, hdr->resource_arg);
        }

        bfree(hdr, hdr->allocation_size, MEMORY_TAG_RESOURCES);
    }

    darray_destroy(system->owned_resources);
}
