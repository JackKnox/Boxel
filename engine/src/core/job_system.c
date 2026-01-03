#include "defines.h"
#include "job_system.h"

#include "utils/darray.h"

b8 job_thread_func(void* arg) {
    job_worker* worker = (job_worker*)arg;

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

b8 job_worker_start(job_worker* worker, PFN_worker_thread func_ptr, u64 size_of_job, void* worker_arg) {
    worker->job_queue = _darray_create(1, size_of_job, MEMORY_TAG_CORE);
    worker->func_ptr = func_ptr;
    worker->worker_arg = worker_arg;
    worker->is_running = TRUE;
    
    if (!cnd_init(&worker->cnd)) {
        BX_ERROR("Failed to init condition variable for worker thread.");
        return FALSE;
    }

    if (!mtx_init(&worker->mutex, mtx_plain)) {
        BX_ERROR("Failed to init mutex for worker thread.");
        return FALSE;
    }

    if (!thrd_create(&worker->resource_thread, job_thread_func, (void*)worker)) {
        BX_ERROR("Failed to create worker thread.");
        return FALSE;
    }

    return TRUE;
}

void job_worker_stop(job_worker* worker) {
    job_worker_quit(worker, FALSE);
    thrd_join(worker->resource_thread, NULL);

    mtx_destroy(&worker->mutex);
    cnd_destroy(&worker->cnd);
    darray_destroy(worker->job_queue);
}

void job_worker_push(job_worker* worker, void* job) {
    if (worker->should_quit) return;

    // Update state and waiting counter under lock to avoid races with wait()
    mtx_lock(&worker->mutex);

    worker->job_queue = _darray_push(worker->job_queue, job);
    cnd_signal(&worker->cnd);

    mtx_unlock(&worker->mutex);
}

void job_worker_wait_until_idle(job_worker* worker) {
    mtx_lock(&worker->mutex);
    while (darray_length(worker->job_queue) > 0 && worker->is_running) 
        cnd_wait(&worker->cnd, &worker->mutex);
    mtx_unlock(&worker->mutex);
}

void job_worker_quit(job_worker* worker, b8 should_quit) {
    mtx_lock(&worker->mutex);
    
    // Wake the resource thread so it can exit
    worker->should_quit = should_quit; // Allows proper destruction on thread.
    worker->is_running = !should_quit; // Forces thread to stop.
    cnd_signal(&worker->cnd);

    mtx_unlock(&worker->mutex);
}
