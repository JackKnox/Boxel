#include "defines.h"
#include "memory.h"

#include "platform/platform.h"

typedef struct memory_stats {
	u64 total_allocated;
	u64 tagged_allocations[MEMORY_TAG_MAX_TAGS];
} memory_stats;

static const char* tag_strings[MEMORY_TAG_MAX_TAGS] = {
    "UNKNOWN   ",
	"ENGINE    ",
	"PLATFORM  ",
	"CORE      ",
	"RESOURCES ",
	"RENDERER  "};

static memory_stats stats;

b8 memory_initialize() {
	bzero_memory(&stats, sizeof(stats));
	return TRUE;
}

void memory_shutdown() {
	if (stats.total_allocated == 0) return;
	for (u32 i = 0; i < MEMORY_TAG_MAX_TAGS; ++i) {
		if (stats.tagged_allocations[i] == 0) continue;
		BX_ERROR("Unfreed %llu bytes on MEMORY_TAG_%s", stats.tagged_allocations[i], tag_strings[i]);
	}
}

void* ballocate(u64 size, memory_tag tag) {
	breport(size, tag);
	return bzero_memory(platform_allocate(size, FALSE), size);
}

void bfree(void* block, u64 size, memory_tag tag) {
	breport_free(size, tag);
	platform_free(block, FALSE);
}

void breport(u64 size, memory_tag tag) {
	stats.total_allocated += size;
	stats.tagged_allocations[tag] += size;
}

void breport_free(u64 size, memory_tag tag) {
	stats.total_allocated -= size;
	stats.tagged_allocations[tag] -= size;
}

void* bzero_memory(void* block, u64 size) {
	return bset_memory(block, 0, size);
}

void* bcopy_memory(void* dest, const void* source, u64 size) {
	return platform_copy_memory(dest, source, size);
}

void* bset_memory(void* dest, i32 value, u64 size) {
	return platform_set_memory(dest, value, size);
}

void print_memory_usage() {
	const u64 gib = 1024 * 1024 * 1024;
	const u64 mib = 1024 * 1024;
	const u64 kib = 1024;

	BX_TRACE("System memory use (tagged):");
	for (u32 i = 0; i < MEMORY_TAG_MAX_TAGS; ++i) {
		char unit[4] = "XiB";
		float amount = 1.0f;
		if (stats.tagged_allocations[i] >= gib) {
			unit[0] = 'G';
			amount = stats.tagged_allocations[i] / (float)gib;
		}
		else if (stats.tagged_allocations[i] >= mib) {
			unit[0] = 'M';
			amount = stats.tagged_allocations[i] / (float)mib;
		}
		else if (stats.tagged_allocations[i] >= kib) {
			unit[0] = 'K';
			amount = stats.tagged_allocations[i] / (float)kib;
		}
		else {
			unit[0] = 'B';
			unit[1] = 0;
			amount = (float)stats.tagged_allocations[i];
		}

		BX_TRACE("  %s | %.2f%s", tag_strings[i], amount, unit);
	}
}