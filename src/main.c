// Project Yurei - High-performance Solana data engine (MIT License)
// Copyright (c) 2025 Project Yurei
// https://x.com/yureiai  PRD: yurei-jsonrpc-client
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "config.h"
#include "db_writer.h"
#include "event_queue.h"
#include "http_poller.h"
#include "logging.h"
#include "websocket_client.h"

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

    YureiEventQueue queue;
    if (yurei_queue_init(&queue, config.queue_capacity) != 0) {
        YUREI_LOG_ERROR("Unable to initialize event queue");
        return 1;
    }

    YureiDbWriter writer;
    if (yurei_db_writer_start(&writer, &config, &queue) != 0) {
        YUREI_LOG_ERROR("Unable to start DB writer thread");
        yurei_queue_destroy(&queue);
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
        if (yurei_http_poller_start(&http_poller, &config, &queue) != 0) {
            YUREI_LOG_ERROR("Failed to start HTTP poller");
        }
    }

    install_signal_handlers();
    YUREI_LOG_INFO("Yurei JSON-RPC client started (mode=%s)", config.rpc_mode);

    while (!g_should_exit) {
        pause();
    }

    YUREI_LOG_INFO("Shutting down...");
    if (use_ws) {
        yurei_ws_client_stop(&ws_client);
    }
    if (use_http && http_poller.running) {
        yurei_http_poller_stop(&http_poller);
    }

    yurei_queue_close(&queue);
    yurei_db_writer_stop(&writer);
    yurei_queue_destroy(&queue);
    return 0;
}
