#include "defines.h" 

#include "utils/darray.h"

typedef struct {
    void* listener;
    PFN_on_event callback;
} registered_event;

typedef struct {
    registered_event* events;
} event_code_entry;

// State structure.
typedef struct {
    // Lookup table for event codes.
    event_code_entry registered[MAX_EVENT_CODE];
} event_system_state;

// Event system internal state.
static b8 is_initialized = FALSE;
static event_system_state* state = NULL;

void event_initialize(burst_allocator* allocator) {
    if (is_initialized == TRUE) {
        return;
    }

    burst_add_block(allocator, sizeof(event_system_state), MEMORY_TAG_CORE, &state);
    is_initialized = TRUE;
}

void event_shutdown() {
    if (!is_initialized) return;

    // Free the events arrays. And objects pointed to should be destroyed on their own.
    for (u16 i = 0; i < MAX_EVENT_CODE; ++i) {
        if (state->registered[i].events != 0) {
            darray_destroy(state->registered[i].events);
            state->registered[i].events = 0;
        }
    }
}

b8 event_register(u16 code, void* listener, PFN_on_event on_event) {
    BX_ASSERT(code > 0 && listener != NULL && on_event != NULL && "Invalid arguments passed to event_register");
    if (is_initialized == FALSE) {
        return FALSE;
    }

    if (state->registered[code].events == 0) {
        state->registered[code].events = darray_create(registered_event, MEMORY_TAG_CORE);
    }

    u64 registered_count = darray_length(state->registered[code].events);
    for (u64 i = 0; i < registered_count; ++i) {
        if (state->registered[code].events[i].listener == listener) {
            BX_WARN("Event listener already registered. Code: %u, Listener: %p", code, listener);
            return FALSE;
        }
    }

    // If at this point, no duplicate was found. Proceed with registration.
    registered_event event;
    event.listener = listener;
    event.callback = on_event;
    state->registered[code].events = _darray_push(state->registered[code].events, &event);

    return TRUE;
}

b8 event_unregister(u16 code, void* listener, PFN_on_event on_event) {
    BX_ASSERT(code > 0 && listener != NULL && on_event != NULL && "Invalid arguments passed to event_unregister");
    if (is_initialized == FALSE) {
        return FALSE;
    }

    // On nothing is registered for the code, boot out.
    if (state->registered[code].events == 0) {
        return FALSE;
    }

    u64 registered_count = darray_length(state->registered[code].events);
    for (u64 i = 0; i < registered_count; ++i) {
        registered_event e = state->registered[code].events[i];
        if (e.listener == listener && e.callback == on_event) {
            // Found one, remove it
            darray_pop_at(state->registered[code].events, i, NULL);
            return TRUE;
        }
    }

    // Not found.
    return FALSE;
}

b8 event_fire(u16 code, void* sender, event_context context) {
    BX_ASSERT(code > 0 && "Invalid arguments passed to event_fire");
    if (is_initialized == FALSE) {
        return FALSE;
    }

    // If nothing is registered for the code, boot out.
    if (state->registered[code].events == 0) {
        return FALSE;
    }

    u64 registered_count = darray_length(state->registered[code].events);
    for (u64 i = 0; i < registered_count; ++i) {
        registered_event* e = &state->registered[code].events[i];
        if (e->callback(code, sender, e->listener, context)) {
            // Message has been handled, do not send to other listeners.
            return TRUE;
        }
    }

    // Not found.
    return FALSE;
}