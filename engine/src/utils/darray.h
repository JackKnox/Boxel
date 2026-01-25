#pragma once

#include "defines.h"

/*
Memory layout
u64 capacity = number elements that can be held
u64 length = number of elements currently contained
u64 stride = size of each element in bytes
u64 memory_tag = memory tag of darray
void* elements
*/

enum {
    DARRAY_CAPACITY,
    DARRAY_LENGTH,
    DARRAY_STRIDE,
    DARRAY_MEMORY_TAG,
    DARRAY_FIELD_LENGTH
};

void* _darray_create(u64 length, u64 stride, memory_tag tag);
void _darray_destroy(void* array);

u64 _darray_field_get(void* array, u64 field);
void _darray_field_set(void* array, u64 field, u64 value);

void* _darray_resize(void* array);

void* _darray_push(void* array, const void* value_ptr);
void* _darray_push_empty(void** array);
void _darray_pop(void* array, void* dest);

void* _darray_pop_at(void* array, u64 index, void* dest);
void* _darray_insert_at(void* array, u64 index, void* value_ptr);

#define DARRAY_DEFAULT_CAPACITY 1
#define DARRAY_RESIZE_FACTOR 2

#define darray_create(type, tag) \
    _darray_create(DARRAY_DEFAULT_CAPACITY, sizeof(type), tag)

#define darray_reserve(type, capacity, tag) \
    _darray_create(capacity, sizeof(type), tag)

#define darray_destroy(array) _darray_destroy(array);

#define darray_push(array, value)           \
    {                                       \
        void* temp = (void*)(value);        \
        array = _darray_push(array, &temp); \
    }

#define darray_push_empty(array) \
    _darray_push_empty((void**) &array)

#define darray_pop(array, value_ptr) \
    _darray_pop(array, value_ptr)

#define darray_insert_at(array, index, value)           \
    {                                                   \
        void* temp = (void*)(value);                    \
        array = _darray_insert_at(array, index, &temp); \
    }

#define darray_pop_at(array, index, value_ptr) \
    _darray_pop_at(array, index, value_ptr)

#define darray_clear(array) \
    _darray_field_set(array, DARRAY_LENGTH, 0)

#define darray_capacity(array) \
    _darray_field_get(array, DARRAY_CAPACITY)

#define darray_length(array) \
    _darray_field_get(array, DARRAY_LENGTH)

#define darray_stride(array) \
    _darray_field_get(array, DARRAY_STRIDE)

#define darray_length_set(array, value) \
    _darray_field_set(array, DARRAY_LENGTH, value)
