#pragma once

#include "defines.h"

// Generic includes
#include <time.h>

// Platform specific includes
#if defined(BX_PLATFORM_POSIX)
#   include <pthread.h>
#elif defined(BX_PLATFORM_WINDOWS)
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#       define __UNDEF_LEAN_AND_MEAN
#   endif
#       include <windows.h>
#       ifdef __UNDEF_LEAN_AND_MEAN
#       undef WIN32_LEAN_AND_MEAN
#       undef __UNDEF_LEAN_AND_MEAN
#   endif
#endif

// If TIME_UTC is missing, provide it and provide a wrapper for timespec_get.
#ifndef TIME_UTC
#define TIME_UTC 1
#define BX_EMULATE_TIMESPEC_GET

#if defined(BX_PLATFORM_WINDOWS)
struct _tthread_timespec {
    time_t tv_sec;
    long   tv_nsec;
};
#define timespec _tthread_timespec
#endif

int _tthread_timespec_get(struct timespec* ts, int base);
#define timespec_get _tthread_timespec_get
#endif

// Mutex types
typedef enum box_mutex_type {
    BOX_MUTEX_TYPE_PLAIN,
    BOX_MUTEX_TYPE_TIMED,
    BOX_MUTEX_TYPE_RECURSIVE,
} box_mutex_type;

// Mutexs
#if defined(BX_PLATFORM_WINDOWS)
typedef struct {
    union {
        CRITICAL_SECTION cs;
        HANDLE mut;         
    } mHandle;
    box_mutex_type mType;
    int mAlreadyLocked;
} box_mutex;
#else
typedef pthread_mutex_t box_mutex;
#endif

// Create a mutex object.
b8 mutex_init(box_mutex* mtx, box_mutex_type type);

//  Release any resources used by the given mutex.
void mutex_destroy(box_mutex* mtx);

// Lock the given mutex. Blocks until the given mutex can be locked.
b8 mutex_lock(box_mutex* mtx);

// Lock the given mutex, or block until a specific point in time.
b8 mutex_timedlock(box_mutex* mtx, const struct timespec* ts);

// Try to lock the given mutex.
b8 mutex_trylock(box_mutex* mtx);

// Unlock the given mutex.
b8 mutex_unlock(box_mutex* mtx);

// Condition variable
#if defined(BX_PLATFORM_WINDOWS)
typedef struct {
    HANDLE mEvents[2];
    unsigned int mWaitersCount;
    CRITICAL_SECTION mWaitersCountLock;
} box_cond;
#else
typedef pthread_cond_t box_cond;
#endif

// Create a condition variable object.
b8 cond_init(box_cond* cond);

// Release any resources used by the given condition variable.
void cond_destroy(box_cond* cond);

// Signal a condition variable.
b8 cond_signal(box_cond* cond);

// Broadcast a condition variable.
b8 cond_broadcast(box_cond* cond);

// Wait for a condition variable to become signaled.
b8 cond_wait(box_cond* cond, box_mutex* mtx);

// Wait for a condition variable to become signaled.
// The function atomically unlocks the given mutex and endeavors to block until
// the given condition variable is signaled by a call to cnd_signal or to
// cnd_broadcast, or until after the specified time. When the calling thread
// becomes unblocked it locks the mutex before it returns.
b8 cond_timedwait(box_cond* cond, box_mutex* mtx, const struct timespec* ts);

// Thread
#if defined(BX_PLATFORM_WINDOWS)
typedef HANDLE box_thread;
#else
typedef pthread_t box_thread;
#endif

// Thread start function.
typedef b8 (*PFN_thread_start)(void* arg);

// Create a new thread.
b8 thread_create(box_thread* thr, PFN_thread_start func, void* arg);

// Identify the calling thread.
box_thread thread_current();

// Dispose of any resources allocated to the thread when that thread exits.
b8 thread_detach(box_thread thr);

// Compare two thread identifiers.
b8 thread_equal(box_thread thr0, box_thread thr1);

// Terminate execution of the calling thread.
BX_NORETURN void thread_exit(int res);

// Wait for a thread to terminate. The function joins the given thread with the current thread by blocking until the other thread has terminated.
b8 thread_join(box_thread thr, int* res);

// Yield execution to another thread. Permit other threads to run, even if the current thread would ordinarily continue to run.
void thread_yield();