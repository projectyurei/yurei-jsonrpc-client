// Project Yurei - High-performance Solana data engine (MIT License)
// Copyright (c) 2025 Project Yurei
// https://x.com/yureiai  PRD: yurei-jsonrpc-client
#include "rate_limiter.h"

#include <stdbool.h>
#include <string.h>
#include <unistd.h>

int yurei_rate_limiter_init(YureiRateLimiter *rl, uint32_t rps) {
    if (!rl) {
        return -1;
    }
    
    memset(rl, 0, sizeof(*rl));
    rl->tokens_per_second = rps;
    
    // Allow burst of up to 2x the rate limit
    rl->max_tokens = (double)rps * 2.0;
    rl->tokens = rl->max_tokens;
    
    clock_gettime(CLOCK_MONOTONIC, &rl->last_refill);
    
    if (pthread_mutex_init(&rl->mutex, NULL) != 0) {
        return -1;
    }
    
    return 0;
}

void yurei_rate_limiter_destroy(YureiRateLimiter *rl) {
    if (!rl) {
        return;
    }
    pthread_mutex_destroy(&rl->mutex);
}

static void refill_tokens(YureiRateLimiter *rl) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    
    double elapsed = (double)(now.tv_sec - rl->last_refill.tv_sec) +
                     (double)(now.tv_nsec - rl->last_refill.tv_nsec) / 1e9;
    
    double new_tokens = elapsed * (double)rl->tokens_per_second;
    rl->tokens += new_tokens;
    
    if (rl->tokens > rl->max_tokens) {
        rl->tokens = rl->max_tokens;
    }
    
    rl->last_refill = now;
}

void yurei_rate_limiter_wait(YureiRateLimiter *rl) {
    if (!rl || rl->tokens_per_second == 0) {
        // Rate limiting disabled
        return;
    }
    
    pthread_mutex_lock(&rl->mutex);
    
    while (true) {
        refill_tokens(rl);
        
        if (rl->tokens >= 1.0) {
            rl->tokens -= 1.0;
            pthread_mutex_unlock(&rl->mutex);
            return;
        }
        
        // Calculate sleep time until next token
        double wait_time = (1.0 - rl->tokens) / (double)rl->tokens_per_second;
        pthread_mutex_unlock(&rl->mutex);
        
        // Sleep for the calculated time (min 1ms, max 100ms)
        uint32_t sleep_us = (uint32_t)(wait_time * 1e6);
        if (sleep_us < 1000) {
            sleep_us = 1000;
        }
        if (sleep_us > 100000) {
            sleep_us = 100000;
        }
        usleep(sleep_us);
        
        pthread_mutex_lock(&rl->mutex);
    }
}

int yurei_rate_limiter_try_acquire(YureiRateLimiter *rl) {
    if (!rl || rl->tokens_per_second == 0) {
        // Rate limiting disabled, always succeed
        return 1;
    }
    
    pthread_mutex_lock(&rl->mutex);
    refill_tokens(rl);
    
    if (rl->tokens >= 1.0) {
        rl->tokens -= 1.0;
        pthread_mutex_unlock(&rl->mutex);
        return 1;
    }
    
    pthread_mutex_unlock(&rl->mutex);
    return 0;
}
