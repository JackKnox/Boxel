#include "defines.h"
#include "platform/platform.h"

#if BX_PLATFORM_LINUX

#include "threading.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Platform specific includes
#include <signal.h>
#include <sched.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>

void* platform_allocate(u64 size, b8 aligned) {
	return malloc(size);
}

void platform_free(const void* block, b8 aligned) {
	free(block);
}

void* platform_copy_memory(void* dest, const void* source, u64 size) {
	return memcpy(dest, source, size);
}

void* platform_set_memory(void* dest, i32 value, u64 size) {
	return memset(dest, value, size);
}

b8 platform_compare_memory(void* buf1, void* buf2, u64 size) {
    return memcmp(buf1, buf2, size) == 0;
}

void platform_console_write(log_level level, const char* message) {
	b8 is_error = (level <= LOG_LEVEL_ERROR);

	static const char* colors[] = {
		"\033[1;37;101m", // fatal
		"\033[1;31m",     // error
		"\033[1;33m",     // warn
		"\033[1;32m",     // info
		"\033[1;36m"      // trace
	};

	printf("%s%s\033[0m\n", colors[level], message);
}

void platform_sleep(u64 ms) {
	nanosleep(ms * 1000.0f);
}

b8 mtx_init(mtx_t* mtx, box_mutex_type type) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    if (mtx->mType & BOX_MUTEX_TYPE_RECURSIVE)
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

    int ret = pthread_mutex_init(mtx, &attr);
    pthread_mutexattr_destroy(&attr);
    return ret == 0 ? TRUE : FALSE;
}

void mtx_destroy(mtx_t* mtx) {
    pthread_mutex_destroy(mtx);
}

b8 mtx_lock(mtx_t* mtx) {
    return pthread_mutex_lock(mtx) == 0 ? TRUE : FALSE;
}

b8 mtx_timedlock(mtx_t* mtx, const struct timespec* ts) {
#if defined(_POSIX_TIMEOUTS) && (_POSIX_TIMEOUTS >= 200112L) && defined(_POSIX_THREADS) && (_POSIX_THREADS >= 200112L)
    switch (pthread_mutex_timedlock(mtx, ts)) {
    case 0:
        return TRUE;
    case ETIMEDOUT:
    default:
        return FALSE;
    }
#else
    int rc;
    struct timespec cur, dur;

    // Try to acquire the lock and, if we fail, sleep for 5ms.
    while ((rc = pthread_mutex_trylock(mtx)) == EBUSY) {
        timespec_get(&cur, TIME_UTC);

        if ((cur.tv_sec > ts->tv_sec) || ((cur.tv_sec == ts->tv_sec) && (cur.tv_nsec >= ts->tv_nsec)))
            break;

        dur.tv_sec = ts->tv_sec - cur.tv_sec;
        dur.tv_nsec = ts->tv_nsec - cur.tv_nsec;
        if (dur.tv_nsec < 0) {
            dur.tv_sec--;
            dur.tv_nsec += 1000000000;
        }

        if ((dur.tv_sec != 0) || (dur.tv_nsec > 5000000)) {
            dur.tv_sec = 0;
            dur.tv_nsec = 5000000;
        }

        nanosleep(&dur, NULL);
    }

    switch (rc) {
    case 0:
        return TRUE;
    case ETIMEDOUT:
    case EBUSY:
    default:
        return FALSE;
    }
#endif
}

b8 mtx_trylock(mtx_t* mtx) {
    return (pthread_mutex_trylock(mtx) == 0) ? TRUE : FALSE;
}

b8 mtx_unlock(mtx_t* mtx) {
    return pthread_mutex_unlock(mtx) == 0 ? TRUE : FALSE;
}

b8 cnd_init(cnd_t* cond) {
    return pthread_cond_init(cond, NULL) == 0 ? TRUE : FALSE;
}

void cnd_destroy(cnd_t* cond) {
    pthread_cond_destroy(cond);
}

b8 cnd_signal(cnd_t* cond) {
    return pthread_cond_signal(cond) == 0 ? TRUE : FALSE;
}

b8 cnd_broadcast(cnd_t* cond) {
    return pthread_cond_broadcast(cond) == 0 ? TRUE : FALSE;
}

b8 cnd_wait(cnd_t* cond, mtx_t* mtx) {
    return pthread_cond_wait(cond, mtx) == 0 ? TRUE : FALSE;
}

b8 cnd_timedwait(cnd_t* cond, mtx_t* mtx, const struct timespec* ts) {
    int ret = pthread_cond_timedwait(cond, mtx, ts);
    if (ret == ETIMEDOUT) return FALSE;

    return ret == 0 ? TRUE : FALSE;
}

// Information to pass to the new thread (what to run).
typedef struct _thread_start_info {
    thrd_start_t mFunction; // Pointer to the function to be executed.
    void* mArg;             // Function argument for the thread function.
} _thread_start_info;

// Thread wrapper function.
static void* _thrd_wrapper_function(void* aArg) {
    thrd_start_t fun;
    void* arg;
    b8  res;

    // Get thread startup information
    _thread_start_info* ti = (_thread_start_info*)aArg;
    fun = ti->mFunction;
    arg = ti->mArg;

    // The thread is responsible for freeing the startup information
    free((void*)ti);

    // Call the actual client thread function
    res = fun(arg);

    return (void*)(intptr_t)res;
}

b8 thrd_create(thrd_t* thr, thrd_start_t func, void* arg) {
    // Fill out the thread startup information (passed to the thread wrapper, which will eventually free it)
    _thread_start_info* ti = (_thread_start_info*)malloc(sizeof(_thread_start_info));
    if (ti == NULL)
        return FALSE;

    ti->mFunction = func;
    ti->mArg = arg;

    // Create the thread
    if (pthread_create(thr, NULL, _thrd_wrapper_function, (void*)ti) != 0)
        *thr = 0;

    // Did we fail to create the thread?
    if (!*thr) {
        free(ti);
        return FALSE;
    }

    return TRUE;
}

thrd_t thrd_current(void) {
    return pthread_self();
}

b8 thrd_detach(thrd_t thr) {
    return pthread_detach(thr) == 0 ? TRUE : FALSE;
}

b8 thrd_equal(thrd_t thr0, thrd_t thr1) {
    return pthread_equal(thr0, thr1);
}

void thrd_exit(int res) {
    pthread_exit((void*)(intptr_t)res);
}

b8 thrd_join(thrd_t thr, int* res) {
    void* pres;
    if (pthread_join(thr, &pres) != 0)
        return FALSE;

    if (res != NULL)
        *res = (int)(intptr_t)pres;

    return TRUE;
}

void thrd_yield(void) {
    sched_yield();
}

#endif