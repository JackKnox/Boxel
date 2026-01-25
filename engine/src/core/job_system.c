#include "defines.h"
#include "job_system.h"

#include "utils/darray.h"

b8 job_worker_on_application_quit(u16 code, void* sender, void* listener_inst, event_context data) {
    job_worker_quit((job_worker*)listener_inst, TRUE);
    return FALSE;
}

b8 job_thread_func(void* arg) {
    job_worker* worker = (job_worker*)arg;
    BX_ASSERT(worker != NULL && "Invalid arguments passed to job_thread_func");

    for (;;) {
        mutex_lock(&worker->mutex);

        // Wait for work to be added to queue.
        while (darray_length(worker->job_queue) == 0 && worker->is_running && !worker->should_quit) 
            cond_wait(&worker->cond, &worker->mutex);

        // Exit immediately, discard remaining jobs
        // Finish-all mode: exit once queue is empty
        // Normal shutdown: no running + no work
        if (!worker->is_running && !worker->should_quit) {
            mutex_unlock(&worker->mutex);
            break;
        }

        if (worker->should_quit && darray_length(worker->job_queue) == 0) {
            mutex_unlock(&worker->mutex);
            break;
        }

        if (!worker->is_running && darray_length(worker->job_queue) == 0) {
            mutex_unlock(&worker->mutex);
            break;
        }

        job_info job_todo = { 0 };
        darray_pop(worker->job_queue, &job_todo);

        mutex_unlock(&worker->mutex);

        b8 success = job_todo.entry_point(worker, &job_todo);
        if (!success)
            BX_WARN("Failed to consume job on worker thread");

        mutex_lock(&worker->mutex);
        if (darray_length(worker->job_queue) == 0)
            cond_broadcast(&worker->cond);
        mutex_unlock(&worker->mutex);
    }

    return TRUE;
}

b8 job_worker_start(job_worker* worker, memory_tag tag) {
    BX_ASSERT(worker != NULL && "Invalid arguments passed to job_worker_start");
    worker->job_queue = darray_create(job_info, tag);
    worker->is_running = TRUE;
    
    if (!cond_init(&worker->cond)) {
        BX_ERROR("Failed to init condition variable for worker thread.");
        return FALSE;
    }

    if (!mutex_init(&worker->mutex, BOX_MUTEX_TYPE_PLAIN)) {
        BX_ERROR("Failed to init mutex for worker thread.");
        return FALSE;
    }

    if (!thread_create(&worker->thread, job_thread_func, (void*)worker)) {
        BX_ERROR("Failed to create worker thread.");
        return FALSE;
    }

    event_register(EVENT_CODE_APPLICATION_QUIT, worker, job_worker_on_application_quit);
    return TRUE;
}

void job_worker_stop(job_worker* worker) {
    BX_ASSERT(worker != NULL && "Invalid arguments passed to job_worker_stop");
    job_worker_quit(worker, TRUE); 

    thread_join(worker->thread, NULL);
    mutex_destroy(&worker->mutex);
    cond_destroy(&worker->cond);
    darray_destroy(worker->job_queue);

    bzero_memory(worker, sizeof(*worker));
}

void job_worker_submit(job_worker* worker, job_info* job) {
    BX_ASSERT(worker != NULL && job != NULL && "Invalid arguments passed to job_worker_submit");
    if (worker->should_quit) return;

    // Update state and waiting counter under lock to avoid races with wait()
    mutex_lock(&worker->mutex);

    worker->job_queue = _darray_push(worker->job_queue, job);
    cond_signal(&worker->cond);

    mutex_unlock(&worker->mutex);
}

void job_worker_wait_until_idle(job_worker* worker) {
    BX_ASSERT(worker != NULL && "Invalid arguments passed to job_worker_wait_until_idle");

    mutex_lock(&worker->mutex);
    while (darray_length(worker->job_queue) > 0 && worker->is_running) 
        cond_wait(&worker->cond, &worker->mutex);
    mutex_unlock(&worker->mutex);
}

void job_worker_quit(job_worker* worker, b8 should_quit) {
    BX_ASSERT(worker != NULL && "Invalid arguments passed to job_worker_quit");

    mutex_lock(&worker->mutex);
    
    // Wake the resource thread so it can exit
    worker->should_quit = should_quit; // Allows proper destruction on thread.
    worker->is_running = !should_quit; // Forces thread to stop.
    cond_signal(&worker->cond);

    mutex_unlock(&worker->mutex);
}
