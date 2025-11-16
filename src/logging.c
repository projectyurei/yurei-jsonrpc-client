// Project Yurei - High-performance Solana data engine (MIT License)
// Copyright (c) 2025 Project Yurei
// https://x.com/yureiai  PRD: yurei-jsonrpc-client
#include "logging.h"

#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

static YureiLogLevel g_level = YUREI_LOG_INFO;
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;

void yurei_log_set_level(YureiLogLevel level) {
    g_level = level;
}

static const char *level_to_string(YureiLogLevel level) {
    switch (level) {
        case YUREI_LOG_DEBUG:
            return "DEBUG";
        case YUREI_LOG_INFO:
            return "INFO";
        case YUREI_LOG_WARN:
            return "WARN";
        case YUREI_LOG_ERROR:
        default:
            return "ERROR";
    }
}

void yurei_log(YureiLogLevel level, const char *fmt, ...) {
    if (level < g_level) {
        return;
    }

    pthread_mutex_lock(&g_log_mutex);
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm_info;
    localtime_r(&ts.tv_sec, &tm_info);
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_info);

    fprintf(stderr, "%s.%03ld [%s] ", buffer, ts.tv_nsec / 1000000L, level_to_string(level));

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    pthread_mutex_unlock(&g_log_mutex);
}
