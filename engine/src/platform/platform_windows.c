#include "defines.h"
#include "platform/platform.h"

#if BX_PLATFORM_WINDOWS

#include "engine.h"
#include "utils/darray.h"

#include <stdlib.h>
#include <stdio.h>
#include <process.h>
#include <sys/timeb.h>

#define _CONDITION_EVENT_ONE 0
#define _CONDITION_EVENT_ALL 1

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
	Sleep(ms);
}

b8 mtx_init(mtx_t* mtx, box_mutex_type type) {
    mtx->mAlreadyLocked = FALSE;
    mtx->mType = type;

    if (!(mtx->mType & BOX_MUTEX_TYPE_TIMED)) {
        InitializeCriticalSection(&(mtx->mHandle.cs));
    }
    else {
        mtx->mHandle.mut = CreateMutex(NULL, FALSE, NULL);
        if (!mtx->mHandle.mut)
            return FALSE;
    }
    return TRUE;
}

void mtx_destroy(mtx_t* mtx) {
    if (!(mtx->mType & BOX_MUTEX_TYPE_TIMED)) {
        DeleteCriticalSection(&(mtx->mHandle.cs));
    }
    else {
        CloseHandle(mtx->mHandle.mut);
    }
}

b8 mtx_lock(mtx_t* mtx) {
    if (!(mtx->mType & BOX_MUTEX_TYPE_TIMED)) {
        EnterCriticalSection(&(mtx->mHandle.cs));
    }
    else {
        switch (WaitForSingleObject(mtx->mHandle.mut, INFINITE)) {
        case WAIT_OBJECT_0:
            break;
        case WAIT_ABANDONED:
        default:
            return FALSE;
        }
    }

    if (!(mtx->mType & BOX_MUTEX_TYPE_RECURSIVE)) {
        while (mtx->mAlreadyLocked) Sleep(1); // Simulate deadlock...
        mtx->mAlreadyLocked = TRUE;
    }
    return TRUE;
}

b8 mtx_timedlock(mtx_t* mtx, const struct timespec* ts) {
    if (!(mtx->mType & BOX_MUTEX_TYPE_TIMED))
        return FALSE;

    struct timespec current_ts;
    DWORD timeoutMs;
    timespec_get(&current_ts, TIME_UTC);

    if ((current_ts.tv_sec > ts->tv_sec) || ((current_ts.tv_sec == ts->tv_sec) && (current_ts.tv_nsec >= ts->tv_nsec))) {
        timeoutMs = 0;
    }
    else {
        timeoutMs = (DWORD)(ts->tv_sec - current_ts.tv_sec) * 1000;
        timeoutMs += (ts->tv_nsec - current_ts.tv_nsec) / 1000000;
        timeoutMs += 1;
    }

    // TODO: the timeout for WaitForSingleObject doesn't include time while the computer is asleep.
    switch (WaitForSingleObject(mtx->mHandle.mut, timeoutMs)) {
    case WAIT_OBJECT_0:
        break;
    case WAIT_TIMEOUT:
    case WAIT_ABANDONED:
    default:
        return FALSE;
    }

    if (!(mtx->mType & BOX_MUTEX_TYPE_RECURSIVE)) {
        while (mtx->mAlreadyLocked) Sleep(1); // Simulate deadlock...
        mtx->mAlreadyLocked = TRUE;
    }

    return TRUE;
}

b8 mtx_trylock(mtx_t* mtx) {
    int ret;

    if (!(mtx->mType & BOX_MUTEX_TYPE_TIMED)) {
        ret = TryEnterCriticalSection(&(mtx->mHandle.cs)) ? TRUE : FALSE;
    }
    else {
        ret = (WaitForSingleObject(mtx->mHandle.mut, 0) == WAIT_OBJECT_0) ? TRUE : FALSE;
    }

    if (!(mtx->mType & BOX_MUTEX_TYPE_RECURSIVE) && (ret == TRUE)) {
        if (mtx->mAlreadyLocked) {
            LeaveCriticalSection(&(mtx->mHandle.cs));
            ret = FALSE;
        }
        else {
            mtx->mAlreadyLocked = TRUE;
        }
    }

    return ret;
}

b8 mtx_unlock(mtx_t* mtx) {
    mtx->mAlreadyLocked = FALSE;
    if (!(mtx->mType & BOX_MUTEX_TYPE_TIMED)) {
        LeaveCriticalSection(&(mtx->mHandle.cs));
    }
    else {
        if (!ReleaseMutex(mtx->mHandle.mut))
            return FALSE;
    }
    return TRUE;
}

b8 cnd_init(cnd_t* cond) {
    cond->mWaitersCount = 0;

    // Init critical section
    InitializeCriticalSection(&cond->mWaitersCountLock);

    // Init events
    cond->mEvents[_CONDITION_EVENT_ONE] = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (cond->mEvents[_CONDITION_EVENT_ONE] == NULL) {
        cond->mEvents[_CONDITION_EVENT_ALL] = NULL;
        return FALSE;
    }
    cond->mEvents[_CONDITION_EVENT_ALL] = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (cond->mEvents[_CONDITION_EVENT_ALL] == NULL) {
        CloseHandle(cond->mEvents[_CONDITION_EVENT_ONE]);
        cond->mEvents[_CONDITION_EVENT_ONE] = NULL;
        return FALSE;
    }

    return TRUE;
}

void cnd_destroy(cnd_t* cond) {
    if (cond->mEvents[_CONDITION_EVENT_ONE] != NULL) {
        CloseHandle(cond->mEvents[_CONDITION_EVENT_ONE]);
    }

    if (cond->mEvents[_CONDITION_EVENT_ALL] != NULL) {
        CloseHandle(cond->mEvents[_CONDITION_EVENT_ALL]);
    }

    DeleteCriticalSection(&cond->mWaitersCountLock);
}

b8 cnd_signal(cnd_t* cond) {
    int haveWaiters;

    // Are there any waiters?
    EnterCriticalSection(&cond->mWaitersCountLock);
    haveWaiters = (cond->mWaitersCount > 0);
    LeaveCriticalSection(&cond->mWaitersCountLock);

    // If we have any waiting threads, send them a signal
    if (haveWaiters) {
        if (SetEvent(cond->mEvents[_CONDITION_EVENT_ONE]) == 0)
            return FALSE;
    }

    return TRUE;
}

b8 cnd_broadcast(cnd_t* cond) {
    int haveWaiters;

    // Are there any waiters?
    EnterCriticalSection(&cond->mWaitersCountLock);
    haveWaiters = (cond->mWaitersCount > 0);
    LeaveCriticalSection(&cond->mWaitersCountLock);

    // If we have any waiting threads, send them a signal
    if (haveWaiters) {
        if (SetEvent(cond->mEvents[_CONDITION_EVENT_ALL]) == 0)
            return FALSE;
    }

    return TRUE;
}

static b8 _cnd_timedwait_win32(cnd_t* cond, mtx_t* mtx, DWORD timeout) {
    DWORD result;
    int lastWaiter;

    // Increment number of waiters 
    EnterCriticalSection(&cond->mWaitersCountLock);
    ++cond->mWaitersCount;
    LeaveCriticalSection(&cond->mWaitersCountLock);

    // Release the mutex while waiting for the condition (will decrease the number of waiters when done)...
    mtx_unlock(mtx);

    // Wait for either event to become signaled due to cnd_signal() or cnd_broadcast() being called
    result = WaitForMultipleObjects(2, cond->mEvents, FALSE, timeout);
    if (result == WAIT_TIMEOUT) {
        // The mutex is locked again before the function returns, even if an error occurred
        mtx_lock(mtx);
        return FALSE;
    }
    else if (result == WAIT_FAILED) {
        // The mutex is locked again before the function returns, even if an error occurred
        mtx_lock(mtx);
        return FALSE;
    }

    // Check if we are the last waiter
    EnterCriticalSection(&cond->mWaitersCountLock);
    --cond->mWaitersCount;
    lastWaiter = (result == (WAIT_OBJECT_0 + _CONDITION_EVENT_ALL)) &&
        (cond->mWaitersCount == 0);
    LeaveCriticalSection(&cond->mWaitersCountLock);

    // If we are the last waiter to be notified to stop waiting, reset the event
    if (lastWaiter) {
        if (ResetEvent(cond->mEvents[_CONDITION_EVENT_ALL]) == 0) {
            // The mutex is locked again before the function returns, even if an error occurred
            mtx_lock(mtx);
            return FALSE;
        }
    }

    // Re-acquire the mutex
    mtx_lock(mtx);

    return TRUE;
}

b8 cnd_wait(cnd_t* cond, mtx_t* mtx) {
    return _cnd_timedwait_win32(cond, mtx, INFINITE);
}

b8 cnd_timedwait(cnd_t* cond, mtx_t* mtx, const struct timespec* ts) {
    struct timespec now;

    if (timespec_get(&now, TIME_UTC) == TIME_UTC) {
        u64 nowInMilliseconds = now.tv_sec * 1000 + now.tv_nsec / 1000000;
        u64 tsInMilliseconds = ts->tv_sec * 1000 + ts->tv_nsec / 1000000;
        DWORD delta = (tsInMilliseconds > nowInMilliseconds) ?
            (DWORD)(tsInMilliseconds - nowInMilliseconds) : 0;
        return _cnd_timedwait_win32(cond, mtx, delta);
    }

    return FALSE;
}

// Information to pass to the new thread (what to run).
typedef struct {
    thrd_start_t mFunction;
    void* mArg;
} _thread_start_info;

// Thread wrapper function.
static DWORD WINAPI _thrd_wrapper_function(LPVOID aArg) {
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
    return (DWORD)res;
}

b8 thrd_create(thrd_t* thr, thrd_start_t func, void* arg) {
    // Fill out the thread startup information (passed to the thread wrapper, which will eventually free it)
    _thread_start_info* ti = (_thread_start_info*)platform_allocate(sizeof(_thread_start_info), FALSE);
    if (ti == NULL)
        return FALSE;

    ti->mFunction = func;
    ti->mArg = arg;

    // Create the thread
    *thr = CreateThread(NULL, 0, _thrd_wrapper_function, (LPVOID)ti, 0, NULL);

    // Did we fail to create the thread? */
    if (!*thr)
    {
        platform_free(ti, FALSE);
        return FALSE;
    }

    return TRUE;
}

thrd_t thrd_current(void) {
    return GetCurrentThread();
}

b8 thrd_detach(thrd_t thr) {
    // https://stackoverflow.com/questions/12744324/how-to-detach-a-thread-on-windows-c#answer-12746081
    return CloseHandle(thr) != 0 ? TRUE : FALSE;
}

b8 thrd_equal(thrd_t thr0, thrd_t thr1) {
    return GetThreadId(thr0) == GetThreadId(thr1);
}

void thrd_exit(int res) {
    ExitThread((DWORD)res);
}

b8 thrd_join(thrd_t thr, int* res) {
    DWORD dwRes;

    if (WaitForSingleObject(thr, INFINITE) == WAIT_FAILED)
        return FALSE;

    if (res != NULL) {
        if (GetExitCodeThread(thr, &dwRes) != 0) {
            *res = (int)dwRes;
        }
        else {
            return FALSE;
        }
    }

    CloseHandle(thr);
    return TRUE;
}

b8 thrd_sleep(const struct timespec* duration, struct timespec* remaining) {
    struct timespec start;
    DWORD t;

    timespec_get(&start, TIME_UTC);

    t = SleepEx((DWORD)(duration->tv_sec * 1000 +
        duration->tv_nsec / 1000000 +
        (((duration->tv_nsec % 1000000) == 0) ? 0 : 1)),
        TRUE);

    if (t == 0) {
        return TRUE;
    }
    else {
        if (remaining != NULL) {
            timespec_get(remaining, TIME_UTC);
            remaining->tv_sec -= start.tv_sec;
            remaining->tv_nsec -= start.tv_nsec;
            if (remaining->tv_nsec < 0)
            {
                remaining->tv_nsec += 1000000000;
                remaining->tv_sec -= 1;
            }
        }

        return (t == WAIT_IO_COMPLETION) ? -1 : -2;
    }
}

void thrd_yield(void) {
    Sleep(0);
}

#endif