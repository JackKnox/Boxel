#pragma once

#include "defines.h"

/* Generic includes */
#include <time.h>

/* Platform specific includes */
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

/* If TIME_UTC is missing, provide it and provide a wrapper for
   timespec_get. */
#ifndef TIME_UTC
#   define TIME_UTC 1
#   define BX_EMULATE_TIMESPEC_GET

#   if defined(BX_PLATFORM_WINDOWS)
    struct _tthread_timespec {
        time_t tv_sec;
        long   tv_nsec;
    };
#       define timespec _tthread_timespec
#   endif

    int _tthread_timespec_get(struct timespec* ts, int base);
#   define timespec_get _tthread_timespec_get
#endif


/* Macros */
#if defined(BX_PLATFORM_WINDOWS)
#   define TSS_DTOR_ITERATIONS (4)
#else
#   define TSS_DTOR_ITERATIONS PTHREAD_DESTRUCTOR_ITERATIONS
#endif

enum {
    thrd_error    = 0,
    thrd_success  = 1,
    thrd_timedout = 2,
    thrd_busy     = 3,
    thrd_nomem    = 4,
};
typedef u32 box_threading_err;

/* Mutex types */
#define mtx_plain     0
#define mtx_timed     1
#define mtx_recursive 2

/* Mutex */
#if defined(BX_PLATFORM_WINDOWS)
typedef struct {
    union {
        CRITICAL_SECTION cs;      /* Critical section handle (used for non-timed mutexes) */
        HANDLE mut;               /* Mutex handle (used for timed mutex) */
    } mHandle;                  /* Mutex handle */
    int mAlreadyLocked;         /* TRUE if the mutex is already locked */
    int mRecursive;             /* TRUE if the mutex is recursive */
    int mTimed;                 /* TRUE if the mutex is timed */
} mtx_t;
#else
typedef pthread_mutex_t mtx_t;
#endif

/** Create a mutex object.
* @param mtx A mutex object.
* @param type Bit-mask that must have one of the following six values:
*   @li @c mtx_plain for a simple non-recursive mutex
*   @li @c mtx_timed for a non-recursive mutex that supports timeout
*   @li @c mtx_plain | @c mtx_recursive (same as @c mtx_plain, but recursive)
*   @li @c mtx_timed | @c mtx_recursive (same as @c mtx_timed, but recursive)
* @return @ref thrd_success on success, or @ref thrd_error if the request could
* not be honored.
*/
int mtx_init(mtx_t* mtx, int type);

/** Release any resources used by the given mutex.
* @param mtx A mutex object.
*/
void mtx_destroy(mtx_t* mtx);

/** Lock the given mutex.
* Blocks until the given mutex can be locked. If the mutex is non-recursive, and
* the calling thread already has a lock on the mutex, this call will block
* forever.
* @param mtx A mutex object.
* @return @ref thrd_success on success, or @ref thrd_error if the request could
* not be honored.
*/
int mtx_lock(mtx_t* mtx);

/** Lock the given mutex, or block until a specific point in time.
* Blocks until either the given mutex can be locked, or the specified TIME_UTC
* based time.
* @param mtx A mutex object.
* @param ts A UTC based calendar time
* @return @ref The mtx_timedlock function returns thrd_success on success, or
* thrd_timedout if the time specified was reached without acquiring the
* requested resource, or thrd_error if the request could not be honored.
*/
int mtx_timedlock(mtx_t* mtx, const struct timespec* ts);

/** Try to lock the given mutex.
* The specified mutex shall support either test and return or timeout. If the
* mutex is already locked, the function returns without blocking.
* @param mtx A mutex object.
* @return @ref thrd_success on success, or @ref thrd_busy if the resource
* requested is already in use, or @ref thrd_error if the request could not be
* honored.
*/
int mtx_trylock(mtx_t* mtx);

/** Unlock the given mutex.
* @param mtx A mutex object.
* @return @ref thrd_success on success, or @ref thrd_error if the request could
* not be honored.
*/
int mtx_unlock(mtx_t* mtx);

    /* Condition variable */
#if defined(BX_PLATFORM_WINDOWS)
typedef struct {
    HANDLE mEvents[2];                  /* Signal and broadcast event HANDLEs. */
    unsigned int mWaitersCount;         /* Count of the number of waiters. */
    CRITICAL_SECTION mWaitersCountLock; /* Serialize access to mWaitersCount. */
} cnd_t;
#else
typedef pthread_cond_t cnd_t;
#endif

/** Create a condition variable object.
* @param cond A condition variable object.
* @return @ref thrd_success on success, or @ref thrd_error if the request could
* not be honored.
*/
int cnd_init(cnd_t* cond);

/** Release any resources used by the given condition variable.
* @param cond A condition variable object.
*/
void cnd_destroy(cnd_t* cond);

/** Signal a condition variable.
* Unblocks one of the threads that are blocked on the given condition variable
* at the time of the call. If no threads are blocked on the condition variable
* at the time of the call, the function does nothing and return success.
* @param cond A condition variable object.
* @return @ref thrd_success on success, or @ref thrd_error if the request could
* not be honored.
*/
int cnd_signal(cnd_t* cond);

/** Broadcast a condition variable.
* Unblocks all of the threads that are blocked on the given condition variable
* at the time of the call. If no threads are blocked on the condition variable
* at the time of the call, the function does nothing and return success.
* @param cond A condition variable object.
* @return @ref thrd_success on success, or @ref thrd_error if the request could
* not be honored.
*/
int cnd_broadcast(cnd_t* cond);

/** Wait for a condition variable to become signaled.
* The function atomically unlocks the given mutex and endeavors to block until
* the given condition variable is signaled by a call to cnd_signal or to
* cnd_broadcast. When the calling thread becomes unblocked it locks the mutex
* before it returns.
* @param cond A condition variable object.
* @param mtx A mutex object.
* @return @ref thrd_success on success, or @ref thrd_error if the request could
* not be honored.
*/
int cnd_wait(cnd_t* cond, mtx_t* mtx);

/** Wait for a condition variable to become signaled.
* The function atomically unlocks the given mutex and endeavors to block until
* the given condition variable is signaled by a call to cnd_signal or to
* cnd_broadcast, or until after the specified time. When the calling thread
* becomes unblocked it locks the mutex before it returns.
* @param cond A condition variable object.
* @param mtx A mutex object.
* @param xt A point in time at which the request will time out (absolute time).
* @return @ref thrd_success upon success, or @ref thrd_timeout if the time
* specified in the call was reached without acquiring the requested resource, or
* @ref thrd_error if the request could not be honored.
*/
int cnd_timedwait(cnd_t* cond, mtx_t* mtx, const struct timespec* ts);

    /* Thread */
#if defined(BX_PLATFORM_WINDOWS)
typedef HANDLE thrd_t;
#else
typedef pthread_t thrd_t;
#endif

/** Thread start function.
* Any thread that is started with the @ref thrd_create() function must be
* started through a function of this type.
* @param arg The thread argument (the @c arg argument of the corresponding
*        @ref thrd_create() call).
* @return The thread return value, which can be obtained by another thread
* by using the @ref thrd_join() function.
*/
typedef int (*thrd_start_t)(void* arg);

/** Create a new thread.
* @param thr Identifier of the newly created thread.
* @param func A function pointer to the function that will be executed in
*        the new thread.
* @param arg An argument to the thread function.
* @return @ref thrd_success on success, or @ref thrd_nomem if no memory could
* be allocated for the thread requested, or @ref thrd_error if the request
* could not be honored.
* @note A thread’s identifier may be reused for a different thread once the
* original thread has exited and either been detached or joined to another
* thread.
*/
int thrd_create(thrd_t* thr, thrd_start_t func, void* arg);

/** Identify the calling thread.
* @return The identifier of the calling thread.
*/
thrd_t thrd_current(void);

/** Dispose of any resources allocated to the thread when that thread exits.
    * @return thrd_success, or thrd_error on error
*/
int thrd_detach(thrd_t thr);

/** Compare two thread identifiers.
* The function determines if two thread identifiers refer to the same thread.
* @return Zero if the two thread identifiers refer to different threads.
* Otherwise a nonzero value is returned.
*/
int thrd_equal(thrd_t thr0, thrd_t thr1);

/** Terminate execution of the calling thread.
* @param res Result code of the calling thread.
*/
BX_NORETURN void thrd_exit(int res);

/** Wait for a thread to terminate.
* The function joins the given thread with the current thread by blocking
* until the other thread has terminated.
* @param thr The thread to join with.
* @param res If this pointer is not NULL, the function will store the result
*        code of the given thread in the integer pointed to by @c res.
* @return @ref thrd_success on success, or @ref thrd_error if the request could
* not be honored.
*/
int thrd_join(thrd_t thr, int* res);

/** Put the calling thread to sleep.
* Suspend execution of the calling thread.
* @param duration  Interval to sleep for
* @param remaining If non-NULL, this parameter will hold the remaining
*                  time until time_point upon return. This will
*                  typically be zero, but if the thread was woken up
*                  by a signal that is not ignored before duration was
*                  reached @c remaining will hold a positive time.
* @return 0 (zero) on successful sleep, -1 if an interrupt occurred,
*         or a negative value if the operation fails.
*/
int thrd_sleep(const struct timespec* duration, struct timespec* remaining);

/** Yield execution to another thread.
* Permit other threads to run, even if the current thread would ordinarily
* continue to run.
*/
void thrd_yield(void);