#include "defines.h"
#include "platform/platform.h"

#ifdef BX_PLATFORM_LINUX

#include "threading.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void* platform_allocate(u64 size, b8 aligned) {
	return malloc(size);
}

void platform_free(const void* block, b8 aligned) {
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
	free(block);
	#pragma GCC diagnostic pop
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
	struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;

    while (nanosleep(&ts, &ts) == -1)
        continue;
}

#endif