// Project Yurei - High-performance Solana data engine (MIT License)
// Copyright (c) 2025 Project Yurei
// https://x.com/yureiai  PRD: yurei-jsonrpc-client
#ifndef YUREI_RATE_LIMITER_H
#define YUREI_RATE_LIMITER_H

#include <pthread.h>
#include <stdint.h>
#include <time.h>

typedef struct {
    uint32_t tokens_per_second;
    double tokens;
    double max_tokens;
    struct timespec last_refill;
    pthread_mutex_t mutex;
} YureiRateLimiter;

// Initialize rate limiter with specified requests per second
// Set rps to 0 to disable rate limiting
int yurei_rate_limiter_init(YureiRateLimiter *rl, uint32_t rps);

// Destroy rate limiter (cleanup mutex)
void yurei_rate_limiter_destroy(YureiRateLimiter *rl);

// Wait until a token is available (blocking)
// Returns immediately if rate limiting is disabled
void yurei_rate_limiter_wait(YureiRateLimiter *rl);

// Try to acquire a token without blocking
// Returns 1 if token acquired, 0 if not available
int yurei_rate_limiter_try_acquire(YureiRateLimiter *rl);

#endif // YUREI_RATE_LIMITER_H
