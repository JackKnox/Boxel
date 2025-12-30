#include "defines.h"
#include "resource_system.h"

#include "engine.h"
#include "utils/darray.h"

#define RESOURCE_MAGIC 0x52

b8 resource_system_on_application_quit(u16 code, void* sender, void* listener_inst, event_context data) {
    box_resource_system* system = (box_resource_system*)listener_inst;
    job_worker_quit(&system->worker, TRUE);
    return FALSE;
}

b8 resource_thread_func(job_worker* worker, void* job, box_resource_system* system /* void* arg */) {
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
    return TRUE;
}

b8 resource_system_init(box_resource_system* system, u64 start_mem) {
    bzero_memory(system, sizeof(box_resource_system));
    freelist_create(start_mem, MEMORY_TAG_RESOURCES, &system->resources);

    if (!job_worker_start(&system->worker, resource_thread_func, sizeof(u64), (void*)system)) {
        BX_ERROR("Could not start resource thread");
        return FALSE;
    }

    event_register(EVENT_CODE_APPLICATION_QUIT, system, resource_system_on_application_quit);
    return TRUE;
}

b8 resource_system_allocate_resource(box_resource_system* system, u64 size, void** out_resource) {
    if (system->resources.size + size > system->resources.capacity) {
        BX_ERROR("Ran out of resource system memory");
        return FALSE;
    }

    *out_resource = freelist_push(&system->resources, size, NULL);
    if (!*out_resource) return FALSE;

    box_resource_header* hdr = (box_resource_header*)*out_resource;
    hdr->magic = RESOURCE_MAGIC;
    hdr->state = BOX_RESOURCE_STATE_UNINITIALIZED;
    return TRUE;
}

void resource_system_signal_upload(box_resource_system* system, void* resource) {
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

void resource_system_wait(box_resource_system* system) {
    job_worker_wait_until_idle(&system->worker);
}

void resource_system_shutdown(box_resource_system* system) {
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
