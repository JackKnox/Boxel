#pragma once

typedef enum log_level {
    LOG_LEVEL_FATAL = 0,
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_INFO = 3,
    LOG_LEVEL_TRACE = 4
} log_level;

void log_output(log_level level, const char* message, ...);

#ifndef BX_FATAL
// Logs an fatal-level message.
#define BX_FATAL(message, ...) log_output(LOG_LEVEL_FATAL, message, ##__VA_ARGS__)
#endif

#ifndef BX_ERROR
// Logs an error-level message.
#define BX_ERROR(message, ...) log_output(LOG_LEVEL_ERROR, message, ##__VA_ARGS__)
#endif

#ifndef BX_WARN
// Logs a warning-level message.
#define BX_WARN(message, ...) log_output(LOG_LEVEL_WARN, message, ##__VA_ARGS__)
#endif

#ifndef BX_INFO
// Logs a info-level message.
#define BX_INFO(message, ...) log_output(LOG_LEVEL_INFO, message, ##__VA_ARGS__)
#endif

#ifndef BX_TRACE
// Logs a trace-level message.
#define BX_TRACE(message, ...) log_output(LOG_LEVEL_TRACE, message, ##__VA_ARGS__)
#endif