#include "defines.h"
#include "memory.h"

#include "platform/platform.h"

#if BOX_ENABLE_MEMORY_TRACKING

typedef struct {
	u64 total_allocated;
	u64 tagged_allocations[MEMORY_TAG_MAX_TAGS];
} memory_stats;

static const char* tag_strings[] = {
    "UNKNOWN   ",
	"ENGINE    ",
	"PLATFORM  ",
	"CORE      ",
	"RESOURCES ",
	"RENDERER  ",
	"TOTAL     "};

static b8 is_initialized = FALSE;
static memory_stats stats = { 0 };

#endif

void memory_shutdown() {
#if BOX_ENABLE_MEMORY_TRACKING
	if (stats.total_allocated == 0) return;
	for (u32 i = 0; i < MEMORY_TAG_MAX_TAGS; ++i) {
		if (stats.tagged_allocations[i] == 0) continue;
		BX_ERROR("Unfreed %llu bytes on MEMORY_TAG_%s", stats.tagged_allocations[i], tag_strings[i]);
	}

#endif
}

void* ballocate(u64 size, memory_tag tag) {
	BX_ASSERT(tag != MEMORY_TAG_UNKNOWN && "Memory allocated on unknown tag");

	breport(size, tag);
	return bzero_memory(platform_allocate(size, FALSE), size);
}

void bfree(const void* block, u64 size, memory_tag tag) {
	breport_free(size, tag);
	platform_free(block, FALSE);
}

void breport(u64 size, memory_tag tag) {
#if BOX_ENABLE_MEMORY_TRACKING
	stats.total_allocated += size;
	stats.tagged_allocations[tag] += size;
#endif
}

void breport_free(u64 size, memory_tag tag) {
#if BOX_ENABLE_MEMORY_TRACKING
	stats.total_allocated -= size;
	stats.tagged_allocations[tag] -= size;
#endif
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
#if BOX_ENABLE_MEMORY_TRACKING
	u64 total = 0;

	const u64 gib = 1024 * 1024 * 1024;
	const u64 mib = 1024 * 1024;
	const u64 kib = 1024;

	BX_TRACE("System memory use (tagged):");
	for (u32 i = 0; i < MEMORY_TAG_MAX_TAGS + 1; ++i) {
		u64 raw_amount = i < MEMORY_TAG_MAX_TAGS ? stats.tagged_allocations[i] : total;
		total += raw_amount;

		const char* unit = "B";
		double amount = (double)raw_amount;

		if (raw_amount >= gib) {
			unit = "GiB";
			amount = raw_amount / (double)gib;
		}
		else if (raw_amount >= mib) {
			unit = "MiB";
			amount = raw_amount / (double)mib;
		}
		else if (raw_amount >= kib) {
			unit = "KiB";
			amount = raw_amount / (double)kib;
		}

		BX_TRACE("  %s | %.2f%s", tag_strings[i], amount, unit);
	}
#endif
}