#pragma once

#include "defines.h"

#include "platform/threading.h"

typedef b8 (*PFN_worker_thread)(struct job_worker* worker, void* job, void* arg);

typedef struct job_worker {
	// Running = job thread actually open or not.
	// Suspended = window minimising / paused and waiting to be resumed.
	// Should quit = Thread is in the process of stopping execution.
	b8 is_running, is_suspended;
	b8 should_quit;

	void* job_queue; // darray
	void* worker_arg;

	thrd_t resource_thread;
	PFN_worker_thread func_ptr;

	mtx_t mutex;
	cnd_t cnd;
} job_worker;

// Initializes internal synchronization objects, and launches a worker thread.
b8 job_worker_start(job_worker* worker, PFN_worker_thread worker_func, u64 size_of_job, void* worker_arg);

// Stops the job worker thread immediately and free allocated memory.
void job_worker_stop(job_worker* worker);

// Pushes a new job onto the worker's job queue and signals the worker thread to wake up.
void job_worker_push(job_worker* worker, void* job);

// Blocks the calling thread until the worker becomes idle.
void job_worker_wait_until_idle(job_worker* worker);

// Signals the worker thread to quit execution. If should_quit is TRUE, the worker will finish processing all queued jobs before exiting. If FALSE, the worker will exit immediately and discard remaining jobs.
void job_worker_quit(job_worker* worker, b8 should_quit);