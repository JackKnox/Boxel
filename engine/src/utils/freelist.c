#include "defines.h"
#include "freelist.h"

#pragma pack(push, 1)
typedef struct freelist_header {
    u64 payload_size;
} freelist_header;
#pragma pack(pop)

void* get_user_memory(freelist* list, void* internal_block) {
    return (u8*)internal_block + sizeof(freelist_header);
}

void freelist_create(u64 start_size, memory_tag tag, freelist* out_list) {
    if (!out_list) return;

    out_list->memory = NULL;
    out_list->size = 0;
    out_list->capacity = 0;
    out_list->tag = tag;

    if (start_size > 0) {
        freelist_resize(out_list, start_size);
    }
}

b8 freelist_empty(freelist* list) {
    return list == NULL || list->capacity == 0;
}

void freelist_destroy(freelist* list) {
    if (!list) return;

    if (list->memory) {
        bfree(list->memory, list->capacity, list->tag);
        list->memory = NULL;
    }

    list->capacity = 0;
    list->size = 0;
}

u64 freelist_size(freelist* list) {
    if (freelist_empty(list)) return 0;
    return list->size;
}

u64 freelist_capacity(freelist* list) {
    if (freelist_empty(list)) return 0;
    return list->capacity;
}

void freelist_resize(freelist* list, u64 new_size) {
    if (!list) return;

    if (list->memory == NULL || new_size > list->capacity) {
        u64 new_capacity = (list->capacity == 0) ? 8 : list->capacity;
        while (new_capacity < new_size) new_capacity *= 2;

        void* new_buffer = ballocate(new_capacity, list->tag);

        if (list->memory) {
            // copy only the used bytes
            platform_copy_memory(new_buffer, list->memory, list->size);
            bfree(list->memory, list->capacity, list->tag);
        }

        list->memory = new_buffer;
        list->capacity = new_capacity;
    }
}

void freelist_reset(freelist* list, b8 zero_memory, b8 free_memory) {
    if (!list || list->memory == NULL) return;
    list->size = 0;

    if (free_memory) {
        bfree(list->memory, list->capacity, list->tag);
        list->memory = NULL;
        list->capacity = 0;
        return;
    }

    if (zero_memory) {
        platform_zero_memory(list->memory, list->capacity);
    }
}

void* freelist_push(freelist* list, u64 block_size, void* memory) {
    if (!list) return NULL;
    if (block_size == 0) return NULL;

    u64 aligned_pos = alignment(list->size, 8ULL);
    u64 padding = aligned_pos - list->size;

    u64 required = padding + sizeof(freelist_header) + block_size;
    freelist_resize(list, list->size + required);

    if (!list->memory) return NULL;

    u8* internal_block = (u8*)list->memory + aligned_pos;
    ((freelist_header*)internal_block)->payload_size = block_size;

    u8* user_ptr = internal_block + sizeof(freelist_header);

    if (memory != NULL) {
        platform_copy_memory(user_ptr, memory, block_size);
    }

    list->size = aligned_pos + sizeof(freelist_header) + block_size;
    return (void*)user_ptr;
}

void* freelist_get(freelist* list, u64 index) {
    if (freelist_empty(list)) return NULL;
    if (!list->memory) return NULL;

    u64 pos = 0;
    for (u64 i = 0; i < index; ++i) {
        if (pos + sizeof(freelist_header) > list->size) return NULL; // out of range / corrupted
        freelist_header* hdr = (freelist_header*)((u8*)list->memory + pos);
        if (hdr->payload_size > list->capacity) return NULL;
        pos += sizeof(freelist_header) + hdr->payload_size;
        pos = alignment(pos, 8ULL);
    }

    if (pos + sizeof(freelist_header) > list->size) return NULL;
    return get_user_memory(list, (u8*)list->memory + pos);
}

b8 freelist_next_block(freelist* list, u8** cursor) {
    if (freelist_empty(list) || !cursor || !list->memory) return FALSE;

    u8* next_block_header = 0;

    if (*cursor == 0) {
        next_block_header = list->memory;
    }
    else {
        freelist_header* current_hdr = (freelist_header*)(*cursor - sizeof(freelist_header));
        next_block_header = *cursor + current_hdr->payload_size;
    }

    u64 memory_limit = (u64)list->memory + list->size;

    if ((u64)next_block_header + sizeof(freelist_header) > memory_limit) {
        return FALSE;
    }

    *cursor = (u8*)alignment((u64)next_block_header + sizeof(freelist_header), 8);
    return TRUE;
}