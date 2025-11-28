#include "defines.h"

#include "utils/string_utils.h"

#include <stdarg.h>

void log_output(log_level level, const char* message, ...) {
    const char* level_strings[] = { "[FATAL]: ", "[ERROR]: ", "[WARN]:  ", "[INFO]:  ", "[TRACE]: " };

    // Format the user message first
    va_list arg_ptr;
    va_start(arg_ptr, message);
    char* formatted = string_format_v(message, arg_ptr);
    va_end(arg_ptr);
    
    char* out_message = string_format("%s%s\n", level_strings[level], formatted);
    string_free(formatted);

    platform_console_write(level, out_message);
    string_free(out_message);

    // Trigger a "debug break" for fatal errors.
    if (level == LOG_LEVEL_FATAL)
        bxdebug_break();
}