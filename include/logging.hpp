// Lightweight, header-only logging with compile-time levels.
// - Zero codegen when disabled via levels below
// - Usage: LOG_ONFI_INFO("Erased block %u", block);
//          LOG_HAL_DEBUG("Sending %u address bytes", n);

#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include "timing.hpp" // for get_timestamp_ns()

// Levels: 0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=TRACE
#ifndef LOG_ONFI_LEVEL
#define LOG_ONFI_LEVEL 0
#endif

#ifndef LOG_HAL_LEVEL
#define LOG_HAL_LEVEL 0
#endif

// Optional: allow redirecting output to a file at runtime when enabled
static inline FILE*& logger_output_slot()
{
    static FILE* out = stderr;
    return out;
}

static inline FILE* logger_output()
{
    return logger_output_slot();
}

static inline void logger_set_output_file(FILE* f)
{
    FILE*& out = logger_output_slot();
    fflush(out);
    out = f ? f : stderr;
    // deliberately no fclose here; caller owns FILE*
}

static inline void logger_vlog(const char* comp, const char* level, const char* fmt, va_list ap)
{
    // Timestamp in microseconds
    uint64_t ts_us = get_timestamp_ns() / 1000;
    FILE* out = logger_output();
    flockfile(out);
    fprintf(out, "[%llu.%06llu] [%s] [%s] ",
            (unsigned long long)(ts_us / 1000000ULL),
            (unsigned long long)(ts_us % 1000000ULL),
            level, comp);
    vfprintf(out, fmt, ap);
    fputc('\n', out);
    funlockfile(out);
}

static inline void logger_log(const char* comp, const char* level, const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    logger_vlog(comp, level, fmt, ap);
    va_end(ap);
}

// ONFI component macros
#if LOG_ONFI_LEVEL >= 5
#define LOG_ONFI_TRACE(fmt, ...) logger_log("onfi", "TRACE", fmt, ##__VA_ARGS__)
#define LOG_ONFI_TRACE_IF(cond, fmt, ...) do { if (cond) logger_log("onfi","TRACE",fmt, ##__VA_ARGS__); } while(0)
#else
#define LOG_ONFI_TRACE(...) do{}while(0)
#define LOG_ONFI_TRACE_IF(...) do{}while(0)
#endif

#if LOG_ONFI_LEVEL >= 4
#define LOG_ONFI_DEBUG(fmt, ...) logger_log("onfi", "DEBUG", fmt, ##__VA_ARGS__)
#define LOG_ONFI_DEBUG_IF(cond, fmt, ...) do { if (cond) logger_log("onfi","DEBUG",fmt, ##__VA_ARGS__); } while(0)
#else
#define LOG_ONFI_DEBUG(...) do{}while(0)
#define LOG_ONFI_DEBUG_IF(...) do{}while(0)
#endif

#if LOG_ONFI_LEVEL >= 3
#define LOG_ONFI_INFO(fmt, ...)  logger_log("onfi", "INFO",  fmt, ##__VA_ARGS__)
#define LOG_ONFI_INFO_IF(cond, fmt, ...) do { if (cond) logger_log("onfi","INFO",fmt, ##__VA_ARGS__); } while(0)
#else
#define LOG_ONFI_INFO(...) do{}while(0)
#define LOG_ONFI_INFO_IF(...) do{}while(0)
#endif

#if LOG_ONFI_LEVEL >= 2
#define LOG_ONFI_WARN(fmt, ...)  logger_log("onfi", "WARN",  fmt, ##__VA_ARGS__)
#define LOG_ONFI_WARN_IF(cond, fmt, ...) do { if (cond) logger_log("onfi","WARN",fmt, ##__VA_ARGS__); } while(0)
#else
#define LOG_ONFI_WARN(...) do{}while(0)
#define LOG_ONFI_WARN_IF(...) do{}while(0)
#endif

#if LOG_ONFI_LEVEL >= 1
#define LOG_ONFI_ERROR(fmt, ...) logger_log("onfi", "ERROR", fmt, ##__VA_ARGS__)
#define LOG_ONFI_ERROR_IF(cond, fmt, ...) do { if (cond) logger_log("onfi","ERROR",fmt, ##__VA_ARGS__); } while(0)
#else
#define LOG_ONFI_ERROR(...) do{}while(0)
#define LOG_ONFI_ERROR_IF(...) do{}while(0)
#endif

// HAL component macros
#if LOG_HAL_LEVEL >= 5
#define LOG_HAL_TRACE(fmt, ...) logger_log("hal", "TRACE", fmt, ##__VA_ARGS__)
#define LOG_HAL_TRACE_IF(cond, fmt, ...) do { if (cond) logger_log("hal","TRACE",fmt, ##__VA_ARGS__); } while(0)
#else
#define LOG_HAL_TRACE(...) do{}while(0)
#define LOG_HAL_TRACE_IF(...) do{}while(0)
#endif

#if LOG_HAL_LEVEL >= 4
#define LOG_HAL_DEBUG(fmt, ...) logger_log("hal", "DEBUG", fmt, ##__VA_ARGS__)
#define LOG_HAL_DEBUG_IF(cond, fmt, ...) do { if (cond) logger_log("hal","DEBUG",fmt, ##__VA_ARGS__); } while(0)
#else
#define LOG_HAL_DEBUG(...) do{}while(0)
#define LOG_HAL_DEBUG_IF(...) do{}while(0)
#endif

#if LOG_HAL_LEVEL >= 3
#define LOG_HAL_INFO(fmt, ...)  logger_log("hal", "INFO",  fmt, ##__VA_ARGS__)
#define LOG_HAL_INFO_IF(cond, fmt, ...) do { if (cond) logger_log("hal","INFO",fmt, ##__VA_ARGS__); } while(0)
#else
#define LOG_HAL_INFO(...) do{}while(0)
#define LOG_HAL_INFO_IF(...) do{}while(0)
#endif

#if LOG_HAL_LEVEL >= 2
#define LOG_HAL_WARN(fmt, ...)  logger_log("hal", "WARN",  fmt, ##__VA_ARGS__)
#define LOG_HAL_WARN_IF(cond, fmt, ...) do { if (cond) logger_log("hal","WARN",fmt, ##__VA_ARGS__); } while(0)
#else
#define LOG_HAL_WARN(...) do{}while(0)
#define LOG_HAL_WARN_IF(...) do{}while(0)
#endif

#if LOG_HAL_LEVEL >= 1
#define LOG_HAL_ERROR(fmt, ...) logger_log("hal", "ERROR", fmt, ##__VA_ARGS__)
#define LOG_HAL_ERROR_IF(cond, fmt, ...) do { if (cond) logger_log("hal","ERROR",fmt, ##__VA_ARGS__); } while(0)
#else
#define LOG_HAL_ERROR(...) do{}while(0)
#define LOG_HAL_ERROR_IF(...) do{}while(0)
#endif

#endif // LOGGING_H
