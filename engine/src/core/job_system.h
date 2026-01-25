#pragma once

#include "defines.h"

#include "platform/threading.h"

struct job_worker;
struct job_info;

// Returns true on successful job execution, false on failure.
typedef b8 (*PFN_job_start)(struct job_worker* worker, struct job_info* job);

// Describes a job to be run.
typedef struct job_info {
	// Job processing callback.
	PFN_job_start entry_point;

	// Data to be passed to the entry point upon execution.
	void* param_data;

	// The size of the data passed to the job.
	u32 param_data_size;
} job_info;

// Represents a single worker thread that processes queued jobs asynchronously.
typedef struct job_worker {
	// Worker state flags:
	// - is_running: True if the worker thread is active and window is open.
	// - is_suspended: True if the worker is paused or window is minimized.
	// - should_quit: True when the worker is requested to stop execution.
	b8 is_running, is_suspended, should_quit;

	// Dynamic array of jobs (darray).
	job_info* job_queue;

	// Thread handle for the worker.
	box_thread thread;

	// Synchronization primitives for queue access and worker signaling.
	box_mutex mutex;
	box_cond cond;
} job_worker;

// Initializes internal synchronization objects, and launches a worker thread.
b8 job_worker_start(job_worker* worker, memory_tag tag);

// Stops the job worker thread immediately and free allocated memory.
void job_worker_stop(job_worker* worker);

// Pushes a new job onto the worker's job queue and signals the worker thread to wake up.
void job_worker_submit(job_worker* worker, job_info* job);

// Blocks the calling thread until the worker becomes idle.
void job_worker_wait_until_idle(job_worker* worker);

// Signals the worker thread to quit execution.
// - should_quit = TRUE: Finish processing all queued jobs before exiting.
// - should_quit = FALSE: Exit immediately, discarding remaining jobs.
void job_worker_quit(job_worker* worker, b8 should_quit);