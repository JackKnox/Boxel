#include "defines.h"
#include "platform/platform.h"

#if BX_PLATFORM_LINUX

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

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
	nanosleep(ms * 1000.0f);
}

#endif