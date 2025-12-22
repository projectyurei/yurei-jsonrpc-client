// Project Yurei - High-performance Solana data engine (MIT License)
// Copyright (c) 2025 Project Yurei
// https://x.com/yureiai  PRD: yurei-jsonrpc-client
#include "logging.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

static YureiLogLevel g_level = YUREI_LOG_INFO;
static bool g_color_enabled = true;
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;

// ANSI color codes
#define ANSI_RESET   "\033[0m"
#define ANSI_GRAY    "\033[90m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_RED     "\033[31m"
#define ANSI_BOLD    "\033[1m"

void yurei_log_set_level(YureiLogLevel level) {
    g_level = level;
}

void yurei_log_set_color(bool enabled) {
    g_color_enabled = enabled;
}

static const char *level_to_string(YureiLogLevel level) {
    switch (level) {
        case YUREI_LOG_TRACE:
            return "TRACE";
        case YUREI_LOG_DEBUG:
            return "DEBUG";
        case YUREI_LOG_INFO:
            return "INFO ";
        case YUREI_LOG_WARN:
            return "WARN ";
        case YUREI_LOG_ERROR:
        default:
            return "ERROR";
    }
}

static const char *level_to_color(YureiLogLevel level) {
    if (!g_color_enabled) {
        return "";
    }
    switch (level) {
        case YUREI_LOG_TRACE:
            return ANSI_GRAY;
        case YUREI_LOG_DEBUG:
            return ANSI_CYAN;
        case YUREI_LOG_INFO:
            return ANSI_GREEN;
        case YUREI_LOG_WARN:
            return ANSI_YELLOW;
        case YUREI_LOG_ERROR:
        default:
            return ANSI_RED ANSI_BOLD;
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

    const char *color = level_to_color(level);
    const char *reset = g_color_enabled ? ANSI_RESET : "";

    // Microsecond precision timestamp
    fprintf(stderr, "%s%s.%06ld [%s]%s ",
            color, buffer, ts.tv_nsec / 1000L,
            level_to_string(level), reset);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    
    pthread_mutex_unlock(&g_log_mutex);
}
