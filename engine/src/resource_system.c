#include "defines.h"
#include "resource_system.h"

#include "engine.h"
#include "utils/darray.h"

#define RESOURCE_MAGIC 0x52

b8 resource_thread_func(job_worker* worker, void* job, box_resource_system* system /* void* arg */) {
    BX_ASSERT(worker != NULL && job != NULL && system != NULL && "Invalid arguments passed to resource_thread_func");
    box_resource_header* resource = (box_resource_header*)((u8*)system->resources.memory + *(u64*)job);

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

b8 box_resource_system_init(box_resource_system* system, u64* memory_usage, u64 start_mem) {
    if (memory_usage != NULL) {
        *memory_usage = sizeof(box_resource_system);
        return TRUE;
    }

    BX_ASSERT(system != NULL && "Invalid arguments passed to box_resource_system_init");
    bzero_memory(system, sizeof(box_resource_system));
    freelist_create(start_mem, MEMORY_TAG_RESOURCES, &system->resources);

    if (!job_worker_start(&system->worker, resource_thread_func, sizeof(u64), MEMORY_TAG_RESOURCES, (void*)system)) {
        BX_ERROR("Could not start resource thread");
        return FALSE;
    }

    return TRUE;
}

void* box_resource_system_allocate_resource(box_resource_system* system, u64 size) {
    BX_ASSERT(system != NULL && size > 0 && "Invalid arguments passed to box_resource_system_allocate_resource");
    if (system->resources.size + size > system->resources.capacity) {
        BX_ERROR("Ran out of resource system memory");
        return NULL;
    }

    box_resource_header* new_resource = freelist_push(&system->resources, size, NULL);
    if (!new_resource) return NULL;

    new_resource->magic = RESOURCE_MAGIC;
    new_resource->state = BOX_RESOURCE_STATE_UNINITIALIZED;
    return new_resource;
}

void box_resource_system_signal_upload(box_resource_system* system, void* resource) {
    BX_ASSERT(system != NULL && resource != NULL && "Invalid arguments passed to box_resource_system_signal_upload");
    box_resource_header* hdr = (box_resource_header*)resource;
    if (!hdr || hdr->magic != RESOURCE_MAGIC) {
        BX_WARN("Invalid resource passed to upload.");
        return;
    }

    if (hdr->state != BOX_RESOURCE_STATE_UNINITIALIZED) {
        BX_WARN("Resource signalled for upload more than once");
        return;
    }

    hdr->state = BOX_RESOURCE_STATE_NEEDS_UPLOAD;

    u64 offset = (u64)((u8*)hdr - (u8*)system->resources.memory);
    job_worker_push(&system->worker, &offset);
}

void box_resource_system_flush_uploads(box_resource_system* system) {
    BX_ASSERT(system != NULL && "Invalid arguments passed to box_resource_system_flush_uploads");
    job_worker_wait_until_idle(&system->worker);
}

void box_resource_system_shutdown(box_resource_system* system) {
    BX_ASSERT(system != NULL && "Invalid arguments passed to box_resource_system_shutdown");
    job_worker_wait_until_idle(&system->worker);
    job_worker_stop(&system->worker);

    // Destroy all resources
    u8* cursor = NULL;
    while (freelist_next_block(&system->resources, &cursor)) {
        box_resource_header* hdr = (box_resource_header*)cursor;
        if (!hdr || hdr->magic != RESOURCE_MAGIC) continue;
        BX_ASSERT(hdr->state != BOX_RESOURCE_STATE_UNINITIALIZED);

        // If resource created local data, call destroy_local
        if (hdr->state == BOX_RESOURCE_STATE_READY || hdr->state == BOX_RESOURCE_STATE_FAILED) {
            hdr->vtable.destroy_local(system, hdr, hdr->resource_arg);
        }
    }

    freelist_destroy(&system->resources);
}
