#include "defines.h"
#include "resource_system.h"

#include "engine.h"
#include "utils/darray.h"

#define RESOURCE_MAGIC 0x52

b8 resource_system_on_application_quit(u16 code, void* sender, void* listener_inst, event_context data) {
    box_resource_system* system = (box_resource_system*)listener_inst;

    system->is_running = FALSE;

    // Wake the resource thread so it can exit
    mtx_lock(&system->mutex);
    cnd_signal(&system->cnd);
    mtx_unlock(&system->mutex);
    return TRUE;
}

int resource_thread_func(void* arg) {
    box_resource_system* system = (box_resource_system*)arg;

    for (;;) {
        mtx_lock(&system->mutex);

        // Wait for work or shutdown
        while (system->is_running && darray_length(system->upload_queue) == 0) {
            cnd_wait(&system->cnd, &system->mutex);
        }

        // If we're shutting down and no work, break out
        if (!system->is_running && darray_length(system->upload_queue) == 0) {
            mtx_unlock(&system->mutex);
            break;
        }

        // Pop one item (we're still holding the mutex)
        u64 offset = 0;
        darray_pop_at(system->upload_queue, 0, &offset);
        box_resource_header* resource = (box_resource_header*)((u8*)system->resources.memory + offset);

        mtx_unlock(&system->mutex);

        if (resource->magic != RESOURCE_MAGIC) {
            BX_WARN("Resource in upload queue had invalid magic; skipping.");
            // Decrement waiting_index and signal waiter
            mtx_lock(&system->mutex);
            if (system->waiting_index > 0) --system->waiting_index;
            cnd_signal(&system->cnd);
            mtx_unlock(&system->mutex);
            continue;
        }

        if (resource->state != BOX_RESOURCE_STATE_NEEDS_UPLOAD) {
            // Not expected state, treat as done
            mtx_lock(&system->mutex);
            if (system->waiting_index > 0) --system->waiting_index;
            cnd_signal(&system->cnd);
            mtx_unlock(&system->mutex);
            continue;
        }

        resource->state = BOX_RESOURCE_STATE_UPLOADING;

        // Perform the create (does its own GPU/local allocations)
        b8 created = resource->vtable.create_local(system, resource, resource->resource_arg);
        if (!created) {
            resource->state = BOX_RESOURCE_STATE_FAILED;
        }
        else {
            resource->state = BOX_RESOURCE_STATE_READY;
        }

        // Notify waiters that one resource finished
        mtx_lock(&system->mutex);
        if (system->waiting_index > 0) --system->waiting_index;
        cnd_signal(&system->cnd);
        mtx_unlock(&system->mutex);
    }

    return 0;
}

b8 resource_system_init(box_resource_system* system, u64 start_mem) {
    platform_zero_memory(system, sizeof(box_resource_system));

    freelist_create(start_mem, &system->resources);
    system->upload_queue = darray_create(u64);

    system->waiting_index = 0;
    system->is_running = TRUE;

    event_register(EVENT_CODE_APPLICATION_QUIT, system, resource_system_on_application_quit);

    if (!cnd_init(&system->cnd)) {
        BX_ERROR("Failed to init condition variable for resource system.");
        return FALSE;
    }

    if (!mtx_init(&system->mutex, mtx_plain)) {
        BX_ERROR("Failed to init mutex for resource system.");
        return FALSE;
    }

    if (!thrd_create(&system->resource_thread, resource_thread_func, (void*)system)) {
        BX_ERROR("Failed to create resource thread!");
        return FALSE;
    }

    return TRUE;
}

b8 resource_system_allocate_resource(box_resource_system* system, u64 size, void** out_resource) {
    *out_resource = freelist_push(&system->resources, size, NULL);
    if (!*out_resource) return FALSE;

    platform_zero_memory(*out_resource, size);

    box_resource_header* hdr = (box_resource_header*)*out_resource;
    hdr->magic = RESOURCE_MAGIC;
    hdr->state = BOX_RESOURCE_STATE_UNINITIALIZED;
    return TRUE;
}

void resource_system_add_dependency(box_resource_system* system, void* parent, void* dependency) {
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

    // Update state and waiting counter under lock to avoid races with wait()
    mtx_lock(&system->mutex);

    hdr->state = BOX_RESOURCE_STATE_NEEDS_UPLOAD;
    ++system->waiting_index;

    u64 offset = (u64)((u8*)hdr - (u8*)system->resources.memory);
    darray_push(system->upload_queue, offset);

    cnd_signal(&system->cnd);
    mtx_unlock(&system->mutex);
}

void resource_system_wait(box_resource_system* system) {
    mtx_lock(&system->mutex);
    while (system->waiting_index > 0) {
        cnd_wait(&system->cnd, &system->mutex);
    }
    mtx_unlock(&system->mutex);
}

void resource_system_destroy_resources(box_resource_system* system) {
    // Stop the thread and wake it up 
    mtx_lock(&system->mutex);
    system->is_running = FALSE;
    cnd_signal(&system->cnd);
    mtx_unlock(&system->mutex);

    thrd_join(system->resource_thread, NULL);

    // Destroy all resources
    u8* cursor = NULL;
    while (freelist_next_block(&system->resources, &cursor)) {
        box_resource_header* hdr = (box_resource_header*)cursor;

        if (!hdr) continue;
        if (hdr->magic != RESOURCE_MAGIC) continue;

        // If resource created local data, call destroy_local
        if ((hdr->state == BOX_RESOURCE_STATE_READY || hdr->state == BOX_RESOURCE_STATE_FAILED) &&
            hdr->vtable.destroy_local) {
            hdr->vtable.destroy_local(system, hdr, hdr->resource_arg);
        }
    }

    freelist_destroy(&system->resources);
    darray_destroy(system->upload_queue);

    mtx_destroy(&system->mutex);
    cnd_destroy(&system->cnd);
}
