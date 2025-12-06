#include "defines.h"
#include "resource_system.h"

#include "engine.h"

b8 resource_system_init(box_resource_system* system, u64 start_mem) {
	freelist_create(start_mem, &system->resources);
	return TRUE;
}

b8 resource_system_new_resource(box_resource_system* system, void* resource, u64 resource_size, void* args) {
	if (!system) return FALSE;
	if (!resource || resource_size <= sizeof(box_resource_header)) {
		// ...
		return FALSE;
	}

	box_resource_header* header = (box_resource_header*)resource;
	header->state = BOX_RESOURCE_STATE_NEEDS_UPLOAD;
	header->resource_arg = args;
	freelist_push(&system->resources, resource_size, resource);
	return TRUE;
}

void resource_thread_func(box_resource_system* system) {
	u8* cursor = 0;
	while (freelist_next_block(&system->resources, &cursor)) {
		box_resource_header* hdr = (box_resource_header*)cursor;
		if (hdr->state != BOX_RESOURCE_STATE_NEEDS_UPLOAD) {
			continue;
		}

		hdr->state = BOX_RESOURCE_STATE_UPLOADING;
		if (!hdr->vtable.create_local(system, (void*)hdr, hdr->resource_arg)) {
			BX_WARN("Resource failed loading: 0x%llu", (u8*)hdr);
			hdr->state = BOX_RESOURCE_STATE_FAILED;
		}
		else {
			hdr->state = BOX_RESOURCE_STATE_READY;
		}
	}
}

void resource_system_destroy_resources(box_resource_system* system) {
	u8* cursor = 0;
	while (freelist_next_block(&system->resources, &cursor)) {
		box_resource_header* hdr = (box_resource_header*)cursor;
		hdr->vtable.destroy_local(system, (void*)hdr, hdr->resource_arg);
	}

	freelist_destroy(&system->resources);
}