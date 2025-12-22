// Project Yurei - High-performance Solana data engine (MIT License)
// Copyright (c) 2025 Project Yurei
// https://x.com/yureiai  PRD: yurei-jsonrpc-client
#include "metrics.h"

#include <inttypes.h>
#include <stdatomic.h>
#include <string.h>

#include "logging.h"

void yurei_metrics_init(YureiMetrics *m) {
    if (!m) {
        return;
    }
    memset(m, 0, sizeof(*m));
    atomic_store(&m->latency_min_us, UINT64_MAX);
}

void yurei_metrics_request(YureiMetrics *m, bool success, uint64_t latency_us) {
    if (!m) {
        return;
    }
    
    atomic_fetch_add(&m->requests_total, 1);
    
    if (success) {
        atomic_fetch_add(&m->requests_success, 1);
    } else {
        atomic_fetch_add(&m->requests_failed, 1);
    }
    
    atomic_fetch_add(&m->latency_sum_us, latency_us);
    atomic_fetch_add(&m->latency_count, 1);
    
    // Update min/max latency (relaxed atomics for performance)
    uint64_t current_min = atomic_load(&m->latency_min_us);
    while (latency_us < current_min) {
        if (atomic_compare_exchange_weak(&m->latency_min_us, &current_min, latency_us)) {
            break;
        }
    }
    
    uint64_t current_max = atomic_load(&m->latency_max_us);
    while (latency_us > current_max) {
        if (atomic_compare_exchange_weak(&m->latency_max_us, &current_max, latency_us)) {
            break;
        }
    }
}

void yurei_metrics_bytes(YureiMetrics *m, uint64_t bytes) {
    if (!m) {
        return;
    }
    atomic_fetch_add(&m->bytes_received, bytes);
}

void yurei_metrics_event(YureiMetrics *m) {
    if (!m) {
        return;
    }
    atomic_fetch_add(&m->events_processed, 1);
}

void yurei_metrics_ws_reconnect(YureiMetrics *m) {
    if (!m) {
        return;
    }
    atomic_fetch_add(&m->ws_reconnects, 1);
}

uint64_t yurei_metrics_avg_latency_us(const YureiMetrics *m) {
    if (!m) {
        return 0;
    }
    uint64_t count = atomic_load(&m->latency_count);
    if (count == 0) {
        return 0;
    }
    return atomic_load(&m->latency_sum_us) / count;
}

void yurei_metrics_log(const YureiMetrics *m) {
    if (!m) {
        return;
    }
    
    uint64_t total = atomic_load(&m->requests_total);
    uint64_t success = atomic_load(&m->requests_success);
    uint64_t failed = atomic_load(&m->requests_failed);
    uint64_t events = atomic_load(&m->events_processed);
    uint64_t bytes = atomic_load(&m->bytes_received);
    uint64_t ws_reconn = atomic_load(&m->ws_reconnects);
    uint64_t avg_lat = yurei_metrics_avg_latency_us(m);
    uint64_t min_lat = atomic_load(&m->latency_min_us);
    uint64_t max_lat = atomic_load(&m->latency_max_us);
    
    // Handle case where no requests have been made
    if (min_lat == UINT64_MAX) {
        min_lat = 0;
    }
    
    double success_rate = total > 0 ? (double)success / total * 100.0 : 0.0;
    double bytes_kb = (double)bytes / 1024.0;
    
    YUREI_LOG_INFO("=== METRICS ===");
    YUREI_LOG_INFO("Requests: total=%" PRIu64 " success=%" PRIu64 " failed=%" PRIu64 " (%.1f%% success)",
                   total, success, failed, success_rate);
    YUREI_LOG_INFO("Latency: avg=%" PRIu64 "us min=%" PRIu64 "us max=%" PRIu64 "us",
                   avg_lat, min_lat, max_lat);
    YUREI_LOG_INFO("Events processed: %" PRIu64 " | Bytes received: %.2f KB | WS reconnects: %" PRIu64,
                   events, bytes_kb, ws_reconn);
}
