#include "defines.h"
#include "resource_system.h"

#include "engine.h"
#include "utils/darray.h"

#define MAGIC_NUMBER 148

b8 resource_system_on_application_quit(u16 code, void* sender, void* listener_inst, event_context data) {
	((box_resource_system*)listener_inst)->is_running = FALSE;
	return TRUE;
}

int resource_thread_func(box_resource_system* system) {
	while (system->is_running) {
		box_resource_header* resource = NULL;

		if (darray_length(system->upload_queue) > 0) {
			darray_pop(system->upload_queue, &resource);

			resource->state = BOX_RESOURCE_STATE_UPLOADING;
			if (!resource->vtable.create_local(system, resource, resource->resource_arg)) {
				resource->state = BOX_RESOURCE_STATE_FAILED;
				goto next_task;
			}

			resource->state = BOX_RESOURCE_STATE_READY;

		next_task:
			system->waiting_index = BX_CLAMP(system->waiting_index - 1, 0, UINT32_MAX);
		}
	}

	return TRUE;
}

b8 resource_system_init(box_resource_system* system, u64 start_mem) {
	freelist_create(start_mem, &system->resources);
	system->upload_queue = darray_create(box_resource_header*);
	system->is_running = TRUE;

	event_register(EVENT_CODE_APPLICATION_QUIT, system, resource_system_on_application_quit);

	if (!cnd_init(&system->resource_thread_cnd) ||
		!mtx_init(&system->resource_thread_mutex, mtx_plain) ||
		!thrd_create(&system->resource_thread, resource_thread_func, (void*)system)) {
		BX_ERROR("Failed to start resource thread!");
		return FALSE;
	}

	return TRUE;
}

b8 resource_system_allocate_resource(box_resource_system* system, u64 resource_size, void** out_resource) {
	*out_resource = freelist_push(&system->resources, resource_size, NULL);
	platform_zero_memory(*out_resource, resource_size);
	((box_resource_header*)*out_resource)->magic = MAGIC_NUMBER;
	return TRUE;
}

void resource_system_signal_upload(box_resource_system* system, void* resource) {
	box_resource_header* hdr = (box_resource_header*)resource;
	if (hdr->magic != MAGIC_NUMBER) {
		BX_WARN("Passed malformed engine resource to resource_system_signal_upload.");
		return;
	}

	hdr->state = BOX_RESOURCE_STATE_NEEDS_UPLOAD;
	darray_push(system->upload_queue, hdr);
}

void resource_system_wait(box_resource_system* system) {
	system->waiting_index = darray_length(system->upload_queue);

	// TODO: Remove!!!
	while (system->waiting_index != 0) {};
}

void resource_system_destroy_resources(box_resource_system* system) {
	u8* cursor = 0;
	while (freelist_next_block(&system->resources, &cursor)) {
		box_resource_header* hdr = (box_resource_header*)cursor;
		hdr->vtable.destroy_local(system, (void*)hdr, hdr->resource_arg);
	}

	freelist_destroy(&system->resources);
}