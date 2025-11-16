// Project Yurei - High-performance Solana data engine (MIT License)
// Copyright (c) 2025 Project Yurei
// https://x.com/yureiai  PRD: yurei-jsonrpc-client
#ifndef YUREI_CONFIG_H
#define YUREI_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#define YUREI_RPC_MODE_WS "ws"
#define YUREI_RPC_MODE_HTTP "http"
#define YUREI_RPC_MODE_DUAL "dual"

typedef struct {
    char rpc_endpoint[256];
    char wss_endpoint[256];
    char rpc_mode[16];
    uint32_t poll_interval_ms;
    uint32_t ws_backoff_ms;
    uint32_t ws_backoff_max_ms;
    size_t queue_capacity;
    uint32_t batch_size;
    char pumpfun_program[64];
    char raydium_program[64];
    char pumpfun_table[64];
    char raydium_table[64];
    char pg_conninfo[512];
    char log_level[16];
} YureiConfig;

int yurei_config_load(const char *path, YureiConfig *config);
void yurei_config_print(const YureiConfig *config);

#endif // YUREI_CONFIG_H
