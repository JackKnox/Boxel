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
} mtx_t;
#else
typedef pthread_mutex_t mtx_t;
#endif

// Create a mutex object.
b8 mtx_init(mtx_t* mtx, box_mutex_type type);

//  Release any resources used by the given mutex.
void mtx_destroy(mtx_t* mtx);

// Lock the given mutex. Blocks until the given mutex can be locked.
b8 mtx_lock(mtx_t* mtx);

// Lock the given mutex, or block until a specific point in time.
b8 mtx_timedlock(mtx_t* mtx, const struct timespec* ts);

// Try to lock the given mutex.
b8 mtx_trylock(mtx_t* mtx);

// Unlock the given mutex.
b8 mtx_unlock(mtx_t* mtx);

// Condition variable
#if defined(BX_PLATFORM_WINDOWS)
typedef struct {
    HANDLE mEvents[2];
    unsigned int mWaitersCount;
    CRITICAL_SECTION mWaitersCountLock;
} cnd_t;
#else
typedef pthread_cond_t cnd_t;
#endif

// Create a condition variable object.
b8 cnd_init(cnd_t* cond);

// Release any resources used by the given condition variable.
void cnd_destroy(cnd_t* cond);

// Signal a condition variable.
b8 cnd_signal(cnd_t* cond);

// Broadcast a condition variable.
b8 cnd_broadcast(cnd_t* cond);

// Wait for a condition variable to become signaled.
b8 cnd_wait(cnd_t* cond, mtx_t* mtx);

// Wait for a condition variable to become signaled.
// The function atomically unlocks the given mutex and endeavors to block until
// the given condition variable is signaled by a call to cnd_signal or to
// cnd_broadcast, or until after the specified time. When the calling thread
// becomes unblocked it locks the mutex before it returns.
b8 cnd_timedwait(cnd_t* cond, mtx_t* mtx, const struct timespec* ts);

// Thread
#if defined(BX_PLATFORM_WINDOWS)
typedef HANDLE thrd_t;
#else
typedef pthread_t thrd_t;
#endif

// Thread start function.
typedef b8 (*thrd_start_t)(void* arg);

// Create a new thread.
b8 thrd_create(thrd_t* thr, thrd_start_t func, void* arg);

// Identify the calling thread.
thrd_t thrd_current();

// Dispose of any resources allocated to the thread when that thread exits.
b8 thrd_detach(thrd_t thr);

// Compare two thread identifiers.
b8 thrd_equal(thrd_t thr0, thrd_t thr1);

// Terminate execution of the calling thread.
BX_NORETURN void thrd_exit(int res);

// Wait for a thread to terminate. The function joins the given thread with the current thread by blocking until the other thread has terminated.
b8 thrd_join(thrd_t thr, int* res);

//  Put the calling thread to sleep.
b8 thrd_sleep(const struct timespec* duration, struct timespec* remaining);

// Yield execution to another thread. Permit other threads to run, even if the current thread would ordinarily continue to run.
void thrd_yield(void);