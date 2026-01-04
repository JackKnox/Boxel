#pragma once

#include "defines.h"

#include "platform/threading.h"

// Returns true on successful job execution, false on failure.
typedef b8 (*PFN_worker_thread)(struct job_worker* worker, void* job, void* arg);

// Represents a single worker thread that processes queued jobs asynchronously.
typedef struct job_worker {
	// Worker state flags:
	// - is_running: True if the worker thread is active and window is open.
	// - is_suspended: True if the worker is paused or window is minimized.
	// - should_quit: True when the worker is requested to stop execution.
	b8 is_running, is_suspended, should_quit;

	// Dynamic array of jobs (darray)
	void* job_queue;

	// User-provided argument passed to each job
	void* worker_arg;

	// Thread handle for the worker
	thrd_t resource_thread;

	// Job processing callback
	PFN_worker_thread func_ptr;

	// Synchronization primitives for queue access and worker signaling
	mtx_t mutex;
	cnd_t cnd;
} job_worker;

// Initializes internal synchronization objects, and launches a worker thread.
b8 job_worker_start(job_worker* worker, PFN_worker_thread worker_func, u64 size_of_job, memory_tag tag, void* worker_arg);

// Stops the job worker thread immediately and free allocated memory.
void job_worker_stop(job_worker* worker);

// Pushes a new job onto the worker's job queue and signals the worker thread to wake up.
void job_worker_push(job_worker* worker, void* job);

// Blocks the calling thread until the worker becomes idle.
void job_worker_wait_until_idle(job_worker* worker);

// Signals the worker thread to quit execution.
// - should_quit = TRUE: Finish processing all queued jobs before exiting.
// - should_quit = FALSE: Exit immediately, discarding remaining jobs.
void job_worker_quit(job_worker* worker, b8 should_quit);