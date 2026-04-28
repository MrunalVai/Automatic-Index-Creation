#ifndef PGAI_LOGGER_H
#define PGAI_LOGGER_H

typedef enum { LOG_DEBUG = 0, LOG_INFO, LOG_WARN, LOG_ERROR } LogLevel;

void logger_init(LogLevel min_level);
void log_msg(LogLevel level, const char *fmt, ...);

#define LOG_D(...) log_msg(LOG_DEBUG, __VA_ARGS__)
#define LOG_I(...) log_msg(LOG_INFO,  __VA_ARGS__)
#define LOG_W(...) log_msg(LOG_WARN,  __VA_ARGS__)
#define LOG_E(...) log_msg(LOG_ERROR, __VA_ARGS__)

#endif
