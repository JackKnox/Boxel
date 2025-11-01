#pragma once

typedef enum log_level {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN = 1,
    LOG_LEVEL_INFO = 2,
    LOG_LEVEL_TRACE = 3
} log_level;

void log_output(log_level level, const char* message, ...);

#ifndef BX_ERROR
// Logs an error-level message.
#define BX_ERROR(message, ...) log_output(LOG_LEVEL_ERROR, message, ##__VA_ARGS__);
#endif

#ifndef BX_WARN
// Logs a warning-level message.
#define BX_WARN(message, ...) log_output(LOG_LEVEL_WARN, message, ##__VA_ARGS__);
#endif

#ifndef BX_INFO
// Logs a info-level message.
#define BX_INFO(message, ...) log_output(LOG_LEVEL_INFO, message, ##__VA_ARGS__);
#endif

#ifndef BX_TRACE
// Logs a trace-level message.
#define BX_TRACE(message, ...) log_output(LOG_LEVEL_TRACE, message, ##__VA_ARGS__);
#endif