#include "defines.h"
#include "platform/platform.h"

#if BX_PLATFORM_LINUX

#include "engine.h"

#include <unistd.h>
#include <pthread.h>

typedef struct box_thread {
	pthread_t handle;
} box_thread;

b8 platform_start(box_platform* state, box_config* app_config)
{
	return 0;
}

void platform_shutdown(box_platform* plat_state)
{
}

b8 platform_pump_messages(box_platform* plat_state)
{
	return 0;
}

void* platform_allocate(u64 size, b8 aligned)
{
	return NULL;
}

void platform_free(void* block, b8 aligned)
{
}

void* platform_zero_memory(void* block, u64 size)
{
	return NULL;
}

void* platform_copy_memory(void* dest, const void* source, u64 size)
{
	return NULL;
}

void* platform_set_memory(void* dest, i32 value, u64 size)
{
	return NULL;
}

void platform_console_write(const char* message, u8 colour)
{
}

void platform_console_write_error(const char* message, u8 colour)
{
}

f64 platform_get_absolute_time()
{
	return 0;
}

void platform_sleep(u64 ms)
{
}

#endif