#include "defines.h"

#include "utils/string_utils.h"
#include "platform/platform.h"

#include <stdarg.h>

void log_output(log_level level, const char* message, ...) {
    const char* level_strings[] = { "[FATAL]: ", "[ERROR]: ", "[WARN]:  ", "[INFO]:  ", "[TRACE]: " };
    BX_ASSERT(level < BX_ARRAYSIZE(level_strings) && message != NULL && "Invalid arguments passed to log_output");

    va_list args;
    va_start(args, message);
    char* formatted = string_format_v(message, args);
    va_end(args);

    char* out_message = string_format("%s%s", level_strings[level], formatted);

    platform_free(formatted, FALSE);
    platform_console_write(level, out_message);
    platform_free(out_message, FALSE);

    if (level == LOG_LEVEL_FATAL)
        bxdebug_break();
}