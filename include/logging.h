// Project Yurei - High-performance Solana data engine (MIT License)
// Copyright (c) 2025 Project Yurei
// https://x.com/yureiai  PRD: yurei-jsonrpc-client
#ifndef YUREI_LOGGING_H
#define YUREI_LOGGING_H

#include <stdarg.h>

typedef enum {
    YUREI_LOG_DEBUG = 0,
    YUREI_LOG_INFO,
    YUREI_LOG_WARN,
    YUREI_LOG_ERROR
} YureiLogLevel;

void yurei_log_set_level(YureiLogLevel level);
void yurei_log(YureiLogLevel level, const char *fmt, ...);

#define YUREI_LOG_DEBUG(fmt, ...) yurei_log(YUREI_LOG_DEBUG, fmt, ##__VA_ARGS__)
#define YUREI_LOG_INFO(fmt, ...) yurei_log(YUREI_LOG_INFO, fmt, ##__VA_ARGS__)
#define YUREI_LOG_WARN(fmt, ...) yurei_log(YUREI_LOG_WARN, fmt, ##__VA_ARGS__)
#define YUREI_LOG_ERROR(fmt, ...) yurei_log(YUREI_LOG_ERROR, fmt, ##__VA_ARGS__)

#endif // YUREI_LOGGING_H
