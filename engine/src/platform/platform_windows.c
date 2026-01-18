#include "defines.h"
#include "platform/platform.h"

#ifdef BX_PLATFORM_WINDOWS

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
	Sleep(ms);
}

b8 mutex_init(box_mutex* mtx, box_mutex_type type) {
    mtx->mAlreadyLocked = FALSE;
    mtx->mType = type;

    if (!(mtx->mType & BOX_MUTEX_TYPE_TIMED)) {
        InitializeCriticalSection(&mtx->mHandle.cs);
    }
    else {
        mtx->mHandle.mut = CreateMutex(NULL, FALSE, NULL);
        if (!mtx->mHandle.mut)
            return FALSE;
    }
    return TRUE;
}

void mutex_destroy(box_mutex* mtx) {
    if (!(mtx->mType & BOX_MUTEX_TYPE_TIMED)) {
        DeleteCriticalSection(&mtx->mHandle.cs);
    }
    else {
        CloseHandle(mtx->mHandle.mut);
    }
}

b8 mutex_lock(box_mutex* mtx) {
    if (!(mtx->mType & BOX_MUTEX_TYPE_TIMED)) {
        EnterCriticalSection(&mtx->mHandle.cs);
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

b8 mutex_timedlock(box_mutex* mtx, const struct timespec* ts) {
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

b8 mutex_trylock(box_mutex* mtx) {
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

b8 mutex_unlock(box_mutex* mtx) {
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

b8 cond_init(box_cond* cond) {
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

void cond_destroy(box_cond* cond) {
    if (cond->mEvents[_CONDITION_EVENT_ONE] != NULL) {
        CloseHandle(cond->mEvents[_CONDITION_EVENT_ONE]);
    }

    if (cond->mEvents[_CONDITION_EVENT_ALL] != NULL) {
        CloseHandle(cond->mEvents[_CONDITION_EVENT_ALL]);
    }

    DeleteCriticalSection(&cond->mWaitersCountLock);
}

b8 cond_signal(box_cond* cond) {
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

b8 cond_broadcast(box_cond* cond) {
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

static b8 _cond_timedwait_win32(box_cond* cond, box_mutex* mtx, DWORD timeout) {
    DWORD result;
    int lastWaiter;

    // Increment number of waiters 
    EnterCriticalSection(&cond->mWaitersCountLock);
    ++cond->mWaitersCount;
    LeaveCriticalSection(&cond->mWaitersCountLock);

    // Release the mutex while waiting for the condition (will decrease the number of waiters when done)...
    mutex_unlock(mtx);

    // Wait for either event to become signaled due to cnd_signal() or cnd_broadcast() being called
    result = WaitForMultipleObjects(2, cond->mEvents, FALSE, timeout);
    if (result == WAIT_TIMEOUT) {
        // The mutex is locked again before the function returns, even if an error occurred
        mutex_lock(mtx);
        return FALSE;
    }
    else if (result == WAIT_FAILED) {
        // The mutex is locked again before the function returns, even if an error occurred
        mutex_lock(mtx);
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
            mutex_lock(mtx);
            return FALSE;
        }
    }

    // Re-acquire the mutex
    mutex_lock(mtx);

    return TRUE;
}

b8 cond_wait(box_cond* cond, box_mutex* mtx) {
    return _cond_timedwait_win32(cond, mtx, INFINITE);
}

b8 cond_timedwait(box_cond* cond, box_mutex* mtx, const struct timespec* ts) {
    struct timespec now;

    if (timespec_get(&now, TIME_UTC) == TIME_UTC) {
        u64 nowInMilliseconds = now.tv_sec * 1000 + now.tv_nsec / 1000000;
        u64 tsInMilliseconds = ts->tv_sec * 1000 + ts->tv_nsec / 1000000;
        DWORD delta = (tsInMilliseconds > nowInMilliseconds) ?
            (DWORD)(tsInMilliseconds - nowInMilliseconds) : 0;
        return _cond_timedwait_win32(cond, mtx, delta);
    }

    return FALSE;
}

// Information to pass to the new thread (what to run).
typedef struct {
    PFN_thread_start function;
    void* arg;
} thread_start_info;

// Thread wrapper function.
static DWORD WINAPI _thrd_wrapper_function(LPVOID aArg) {
    PFN_thread_start fun;
    void* arg;
    b8  res;

    // Get thread startup information
    thread_start_info* ti = (thread_start_info*)aArg;
    fun = ti->function;
    arg = ti->arg;

    // The thread is responsible for freeing the startup information
    free((void*)ti);

    // Call the actual client thread function
    res = fun(arg);
    return (DWORD)res;
}

b8 thread_create(box_thread* thr, PFN_thread_start func, void* arg) {
    // Fill out the thread startup information (passed to the thread wrapper, which will eventually free it)
    thread_start_info* ti = (thread_start_info*)platform_allocate(sizeof(thread_start_info), FALSE);
    if (ti == NULL)
        return FALSE;

    ti->function = func;
    ti->arg = arg;

    // Create the thread
    *thr = CreateThread(NULL, 0, _thrd_wrapper_function, (LPVOID)ti, 0, NULL);

    // Did we fail to create the thread?
    if (!*thr)
    {
        platform_free(ti, FALSE);
        return FALSE;
    }

    return TRUE;
}

box_thread thread_current(void) {
    return GetCurrentThread();
}

b8 thread_detach(box_thread thr) {
    return CloseHandle(thr) != 0 ? TRUE : FALSE;
}

b8 thread_equal(box_thread thr0, box_thread thr1) {
    return GetThreadId(thr0) == GetThreadId(thr1);
}

void thread_exit(int res) {
    ExitThread((DWORD)res);
}

b8 thread_join(box_thread thr, int* res) {
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

void thread_yield(void) {
    Sleep(0);
}

#endif