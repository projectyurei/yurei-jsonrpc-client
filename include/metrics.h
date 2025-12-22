// Project Yurei - High-performance Solana data engine (MIT License)
// Copyright (c) 2025 Project Yurei
// https://x.com/yureiai  PRD: yurei-jsonrpc-client
#ifndef YUREI_METRICS_H
#define YUREI_METRICS_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    _Atomic uint64_t requests_total;
    _Atomic uint64_t requests_success;
    _Atomic uint64_t requests_failed;
    _Atomic uint64_t events_processed;
    _Atomic uint64_t latency_sum_us;
    _Atomic uint64_t latency_count;
    _Atomic uint64_t latency_min_us;
    _Atomic uint64_t latency_max_us;
    _Atomic uint64_t bytes_received;
    _Atomic uint64_t ws_reconnects;
} YureiMetrics;

// Initialize metrics to zero
void yurei_metrics_init(YureiMetrics *m);

// Record a request with latency (in microseconds)
void yurei_metrics_request(YureiMetrics *m, bool success, uint64_t latency_us);

// Record bytes received
void yurei_metrics_bytes(YureiMetrics *m, uint64_t bytes);

// Record an event processed
void yurei_metrics_event(YureiMetrics *m);

// Record a WebSocket reconnection
void yurei_metrics_ws_reconnect(YureiMetrics *m);

// Log current metrics summary
void yurei_metrics_log(const YureiMetrics *m);

// Get average latency in microseconds (0 if no requests)
uint64_t yurei_metrics_avg_latency_us(const YureiMetrics *m);

#endif // YUREI_METRICS_H
