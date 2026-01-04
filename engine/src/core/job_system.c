#include "defines.h"
#include "job_system.h"

#include "utils/darray.h"

b8 job_worker_on_application_quit(u16 code, void* sender, void* listener_inst, event_context data) {
    job_worker_quit((job_worker*)listener_inst, TRUE);
    return FALSE;
}

b8 job_thread_func(void* arg) {
    job_worker* worker = (job_worker*)arg;
    BX_ASSERT(worker != NULL && "Invalid job_worker passed to thread argument");

    for (;;) {
        mtx_lock(&worker->mutex);

        // Wait for work to be added to queue.
        while (darray_length(worker->job_queue) == 0 && worker->is_running && !worker->should_quit) 
            cnd_wait(&worker->cnd, &worker->mutex);

        // Exit immediately, discard remaining jobs
        // Finish-all mode: exit once queue is empty
        // Normal shutdown: no running + no work
        if (!worker->is_running && !worker->should_quit) {
            mtx_unlock(&worker->mutex);
            break;
        }

        if (worker->should_quit && darray_length(worker->job_queue) == 0) {
            mtx_unlock(&worker->mutex);
            break;
        }

        if (!worker->is_running && darray_length(worker->job_queue) == 0) {
            mtx_unlock(&worker->mutex);
            break;
        }

        // worker->job_queue == worker->job_queue[0] and already verifed size.
        void* job = worker->job_queue;
        mtx_unlock(&worker->mutex);

        b8 success = worker->func_ptr(worker, job, worker->worker_arg);
        if (!success)
            BX_WARN("Failed to consume job on worker thread");

        mtx_lock(&worker->mutex);
        darray_pop_at(worker->job_queue, 0, NULL);

        if (darray_length(worker->job_queue) == 0)
            cnd_broadcast(&worker->cnd);
        
        mtx_unlock(&worker->mutex);
    }

    return TRUE;
}

b8 job_worker_start(job_worker* worker, PFN_worker_thread func_ptr, u64 size_of_job, memory_tag tag, void* worker_arg) {
    BX_ASSERT(worker != NULL && func_ptr != NULL && size_of_job != 0 && "Invalid arguments passed to job_worker_start");
    worker->job_queue = _darray_create(1, size_of_job, tag);
    worker->func_ptr = func_ptr;
    worker->worker_arg = worker_arg;
    worker->is_running = TRUE;
    
    if (!cnd_init(&worker->cnd)) {
        BX_ERROR("Failed to init condition variable for worker thread.");
        return FALSE;
    }

    if (!mtx_init(&worker->mutex, BOX_MUTEX_TYPE_PLAIN)) {
        BX_ERROR("Failed to init mutex for worker thread.");
        return FALSE;
    }

    if (!thrd_create(&worker->resource_thread, job_thread_func, (void*)worker)) {
        BX_ERROR("Failed to create worker thread.");
        return FALSE;
    }

    event_register(EVENT_CODE_APPLICATION_QUIT, worker, job_worker_on_application_quit);
    return TRUE;
}

void job_worker_stop(job_worker* worker) {
    BX_ASSERT(worker != NULL && "Invalid arguments passed to job_worker_stop");
    job_worker_quit(worker, TRUE); 
    thrd_join(worker->resource_thread, NULL);

    mtx_destroy(&worker->mutex);
    cnd_destroy(&worker->cnd);
    darray_destroy(worker->job_queue);

    bzero_memory(worker, sizeof(*worker));
}

void job_worker_push(job_worker* worker, void* job) {
    BX_ASSERT(worker != NULL && job != NULL && "Invalid arguments passed to job_worker_push");
    if (worker->should_quit) return;

    // Update state and waiting counter under lock to avoid races with wait()
    mtx_lock(&worker->mutex);

    worker->job_queue = _darray_push(worker->job_queue, job);
    cnd_signal(&worker->cnd);

    mtx_unlock(&worker->mutex);
}

void job_worker_wait_until_idle(job_worker* worker) {
    BX_ASSERT(worker != NULL && "Invalid arguments passed to job_worker_push");

    mtx_lock(&worker->mutex);
    while (darray_length(worker->job_queue) > 0 && worker->is_running) 
        cnd_wait(&worker->cnd, &worker->mutex);
    mtx_unlock(&worker->mutex);
}

void job_worker_quit(job_worker* worker, b8 should_quit) {
    BX_ASSERT(worker != NULL && "Invalid arguments passed to job_worker_push");

    mtx_lock(&worker->mutex);
    
    // Wake the resource thread so it can exit
    worker->should_quit = should_quit; // Allows proper destruction on thread.
    worker->is_running = !should_quit; // Forces thread to stop.
    cnd_signal(&worker->cnd);

    mtx_unlock(&worker->mutex);
}
