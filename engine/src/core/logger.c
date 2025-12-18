#include "defines.h"

#include "utils/string_utils.h"

#include <stdarg.h>

void log_output(log_level level, const char* message, ...) {
    const char* level_strings[] = { "[FATAL]: ", "[ERROR]: ", "[WARN]:  ", "[INFO]:  ", "[TRACE]: " };

    u64 formatted_length, message_length;

    // Format the user message first
    va_list arg_ptr;
    va_start(arg_ptr, message);
    char* formatted = string_format_v(message, &formatted_length, arg_ptr);
    va_end(arg_ptr);
    
    char* out_message = string_format("%s%s\n", &message_length, level_strings[level], formatted);
    bfree(formatted, formatted_length, MEMORY_TAG_CORE);

    platform_console_write(level, out_message);
    bfree(out_message, message_length, MEMORY_TAG_CORE);

    // Trigger a "debug break" for fatal errors.
    if (level == LOG_LEVEL_FATAL)
        bxdebug_break();
}