// Project Yurei - High-performance Solana data engine (MIT License)
// Copyright (c) 2025 Project Yurei
// https://x.com/yureiai  PRD: yurei-jsonrpc-client
#ifndef YUREI_WEBSOCKET_CLIENT_H
#define YUREI_WEBSOCKET_CLIENT_H

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

#include "config.h"
#include "event_queue.h"

typedef struct {
    void *context;
    void *wsi;
    pthread_t thread;
    YureiEventQueue *queue;
    const YureiConfig *config;
    bool running;
    bool connected;
    uint32_t backoff_ms;
} YureiWebsocketClient;

int yurei_ws_client_start(YureiWebsocketClient *client,
                          const YureiConfig *config,
                          YureiEventQueue *queue);
void yurei_ws_client_stop(YureiWebsocketClient *client);

#endif // YUREI_WEBSOCKET_CLIENT_H
