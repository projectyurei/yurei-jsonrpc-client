// Project Yurei - High-performance Solana data engine (MIT License)
// Copyright (c) 2025 Project Yurei
// https://x.com/yureiai  PRD: yurei-jsonrpc-client
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "db_writer.h"
#include "event_queue.h"
#include "http_poller.h"
#include "logging.h"
#include "metrics.h"
#include "rate_limiter.h"
#include "websocket_client.h"

#define YUREI_VERSION "1.1.0"
#define METRICS_LOG_INTERVAL_SEC 60

static volatile sig_atomic_t g_should_exit = 0;

static void handle_signal(int signum) {
    (void)signum;
    g_should_exit = 1;
}

static void install_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static void print_startup_banner(const YureiConfig *config) {
    YUREI_LOG_INFO("╔════════════════════════════════════════════════════════════╗");
    YUREI_LOG_INFO("║             PROJECT YUREI JSON-RPC CLIENT v%s            ║", YUREI_VERSION);
    YUREI_LOG_INFO("║        High-performance Solana data ingestion engine       ║");
    YUREI_LOG_INFO("╚════════════════════════════════════════════════════════════╝");
    YUREI_LOG_INFO("RPC endpoint: %s", config->rpc_endpoint);
    YUREI_LOG_INFO("WSS endpoint: %s", config->wss_endpoint);
    YUREI_LOG_INFO("Mode: %s | Batch size: %u | Rate limit: %u rps",
                   config->rpc_mode, config->batch_size, config->rate_limit_rps);
    YUREI_LOG_INFO("Queue capacity: %zu | Log color: %s",
                   config->queue_capacity, config->log_color ? "enabled" : "disabled");
}

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [-c path/to/.env]\n", prog);
}

int main(int argc, char **argv) {
    const char *env_path = ".env";
    for (int i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) &&
            i + 1 < argc) {
            env_path = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("yurei-jsonrpc-client v%s\n", YUREI_VERSION);
            return 0;
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    YureiConfig config;
    if (yurei_config_load(env_path, &config) != 0) {
        YUREI_LOG_ERROR("Failed to load configuration");
        return 1;
    }

    print_startup_banner(&config);

    // Initialize metrics
    YureiMetrics metrics;
    yurei_metrics_init(&metrics);
    YUREI_LOG_DEBUG("Metrics system initialized");

    // Initialize rate limiter
    YureiRateLimiter rate_limiter;
    if (yurei_rate_limiter_init(&rate_limiter, config.rate_limit_rps) != 0) {
        YUREI_LOG_ERROR("Failed to initialize rate limiter");
        return 1;
    }
    YUREI_LOG_DEBUG("Rate limiter initialized: %u rps", config.rate_limit_rps);

    YureiEventQueue queue;
    if (yurei_queue_init(&queue, config.queue_capacity) != 0) {
        YUREI_LOG_ERROR("Unable to initialize event queue");
        yurei_rate_limiter_destroy(&rate_limiter);
        return 1;
    }

    YureiDbWriter writer;
    if (yurei_db_writer_start(&writer, &config, &queue) != 0) {
        YUREI_LOG_ERROR("Unable to start DB writer thread");
        yurei_queue_destroy(&queue);
        yurei_rate_limiter_destroy(&rate_limiter);
        return 1;
    }

    bool use_ws = strcasecmp(config.rpc_mode, YUREI_RPC_MODE_HTTP) != 0;
    bool use_http = strcasecmp(config.rpc_mode, YUREI_RPC_MODE_HTTP) == 0 ||
                    strcasecmp(config.rpc_mode, YUREI_RPC_MODE_DUAL) == 0;

    YureiWebsocketClient ws_client;
    memset(&ws_client, 0, sizeof(ws_client));
    if (use_ws) {
        if (yurei_ws_client_start(&ws_client, &config, &queue) != 0) {
            YUREI_LOG_WARN("Failed to start WebSocket client; falling back to HTTP");
            use_ws = false;
            use_http = true;
        }
    }

    YureiHttpPoller http_poller;
    memset(&http_poller, 0, sizeof(http_poller));
    if (use_http) {
        if (yurei_http_poller_start(&http_poller, &config, &queue, &metrics, &rate_limiter) != 0) {
            YUREI_LOG_ERROR("Failed to start HTTP poller");
        }
    }

    install_signal_handlers();
    YUREI_LOG_INFO("Yurei JSON-RPC client started successfully (mode=%s)", config.rpc_mode);

    // Main loop with periodic metrics logging
    time_t last_metrics_log = time(NULL);
    while (!g_should_exit) {
        sleep(1);
        
        time_t now = time(NULL);
        if (now - last_metrics_log >= METRICS_LOG_INTERVAL_SEC) {
            yurei_metrics_log(&metrics);
            last_metrics_log = now;
        }
    }

    YUREI_LOG_INFO("Shutting down...");
    
    // Final metrics log
    YUREI_LOG_INFO("Final metrics before shutdown:");
    yurei_metrics_log(&metrics);
    
    if (use_ws) {
        yurei_ws_client_stop(&ws_client);
    }
    if (use_http && http_poller.running) {
        yurei_http_poller_stop(&http_poller);
    }

    yurei_queue_close(&queue);
    yurei_db_writer_stop(&writer);
    yurei_queue_destroy(&queue);
    yurei_rate_limiter_destroy(&rate_limiter);
    
    YUREI_LOG_INFO("Shutdown complete.");
    return 0;
}

