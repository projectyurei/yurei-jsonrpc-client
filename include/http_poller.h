// Project Yurei - High-performance Solana data engine (MIT License)
// Copyright (c) 2025 Project Yurei
// https://x.com/yureiai  PRD: yurei-jsonrpc-client
#ifndef YUREI_HTTP_POLLER_H
#define YUREI_HTTP_POLLER_H

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

#include "config.h"
#include "event_queue.h"
#include "metrics.h"
#include "rate_limiter.h"

typedef struct {
    bool running;
    pthread_t thread;
    const YureiConfig *config;
    YureiEventQueue *queue;
    YureiMetrics *metrics;
    YureiRateLimiter *rate_limiter;
    uint64_t last_slot;
} YureiHttpPoller;

int yurei_http_poller_start(YureiHttpPoller *poller,
                            const YureiConfig *config,
                            YureiEventQueue *queue,
                            YureiMetrics *metrics,
                            YureiRateLimiter *rate_limiter);
void yurei_http_poller_stop(YureiHttpPoller *poller);

#endif // YUREI_HTTP_POLLER_H
