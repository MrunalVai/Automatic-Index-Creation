#include "logger.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static LogLevel g_min = LOG_INFO;

void logger_init(LogLevel min_level) { g_min = min_level; }

static const char *level_str(LogLevel l) {
    switch (l) {
    case LOG_DEBUG: return "DEBUG";
    case LOG_INFO:  return "INFO ";
    case LOG_WARN:  return "WARN ";
    case LOG_ERROR: return "ERROR";
    }
    return "?";
}

void log_msg(LogLevel level, const char *fmt, ...) {
    if (level < g_min) return;
    char ts[32];
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);
    fprintf(stderr, "%s [%s] ", ts, level_str(level));
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}
