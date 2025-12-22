// Project Yurei - High-performance Solana data engine (MIT License)
// Copyright (c) 2025 Project Yurei
// https://x.com/yureiai  PRD: yurei-jsonrpc-client
#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "logging.h"

static void copy_string(char *dst, size_t dst_len, const char *value) {
    if (!dst || !value) {
        return;
    }
    snprintf(dst, dst_len, "%s", value);
}

static const char *strip_quotes(const char *value, char *buffer, size_t buffer_len) {
    if (!value || !buffer || buffer_len == 0) {
        return value;
    }
    size_t len = strlen(value);
    if (len >= 2) {
        char first = value[0];
        char last = value[len - 1];
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            size_t copy_len = len - 2;
            if (copy_len >= buffer_len) {
                copy_len = buffer_len - 1;
            }
            memcpy(buffer, value + 1, copy_len);
            buffer[copy_len] = '\0';
            return buffer;
        }
    }
    return value;
}

static void apply_defaults(YureiConfig *config) {
    memset(config, 0, sizeof(*config));
    // Default to Helius RPC (API key appended if provided)
    copy_string(config->rpc_endpoint, sizeof(config->rpc_endpoint),
                "https://mainnet.helius-rpc.com");
    copy_string(config->wss_endpoint, sizeof(config->wss_endpoint),
                "wss://mainnet.helius-rpc.com");
    config->rpc_api_key[0] = '\0';  // No API key by default
    copy_string(config->rpc_mode, sizeof(config->rpc_mode), YUREI_RPC_MODE_WS);
    config->poll_interval_ms = 1000;
    config->ws_backoff_ms = 1000;
    config->ws_backoff_max_ms = 60000;
    config->queue_capacity = 1024;
    config->batch_size = 20;  // Optimized for JSON-RPC batch calls
    config->rate_limit_rps = 10;  // Default 10 requests/second
    config->log_color = true;  // ANSI colors enabled by default
    copy_string(config->pumpfun_program, sizeof(config->pumpfun_program),
                "6EF8rrecthR5Dkzon8Nwu78hRvfCKubJ14M5uBEwF6P");
    copy_string(config->raydium_program, sizeof(config->raydium_program),
                "675kPX9MHTjS2zt1qfr1NYHuzeLXfQM9H24wFSUt1Mp8");
    copy_string(config->pumpfun_table, sizeof(config->pumpfun_table),
                "pumpfun_trades");
    copy_string(config->raydium_table, sizeof(config->raydium_table),
                "raydium_swaps");
    copy_string(config->pg_conninfo, sizeof(config->pg_conninfo),
                "host=127.0.0.1 port=5432 dbname=yurei user=yurei password=secret");
    copy_string(config->log_level, sizeof(config->log_level), "info");
}

static const char *trim(const char *input, char *buffer, size_t buffer_len) {
    if (!input) {
        return NULL;
    }
    const char *start = input;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    size_t end_index = strlen(start);
    while (end_index > 0 && isspace((unsigned char)start[end_index - 1])) {
        end_index--;
    }
    size_t len = end_index;
    if (len >= buffer_len) {
        len = buffer_len - 1;
    }
    memcpy(buffer, start, len);
    buffer[len] = '\0';
    return buffer;
}

static void set_numeric_uint32(uint32_t *field, const char *value) {
    if (!field || !value || !*value) {
        return;
    }
    char *end = NULL;
    unsigned long parsed = strtoul(value, &end, 10);
    if (end && *end == '\0') {
        *field = (uint32_t)parsed;
    }
}

static void set_numeric_size(size_t *field, const char *value) {
    if (!field || !value || !*value) {
        return;
    }
    char *end = NULL;
    unsigned long parsed = strtoul(value, &end, 10);
    if (end && *end == '\0') {
        *field = (size_t)parsed;
    }
}

static void apply_key_value(YureiConfig *config, const char *key, const char *value) {
    if (!config || !key || !value) {
        return;
    }
    char unquoted[512];
    const char *normalized = strip_quotes(value, unquoted, sizeof(unquoted));

    if (strcasecmp(key, "YUREI_RPC_ENDPOINT") == 0) {
        copy_string(config->rpc_endpoint, sizeof(config->rpc_endpoint), normalized);
    } else if (strcasecmp(key, "YUREI_WSS_ENDPOINT") == 0) {
        copy_string(config->wss_endpoint, sizeof(config->wss_endpoint), normalized);
    } else if (strcasecmp(key, "YUREI_RPC_MODE") == 0) {
        copy_string(config->rpc_mode, sizeof(config->rpc_mode), normalized);
    } else if (strcasecmp(key, "YUREI_POLL_INTERVAL_MS") == 0) {
        set_numeric_uint32(&config->poll_interval_ms, normalized);
    } else if (strcasecmp(key, "YUREI_WS_BACKOFF_MS") == 0) {
        set_numeric_uint32(&config->ws_backoff_ms, normalized);
    } else if (strcasecmp(key, "YUREI_WS_BACKOFF_MAX_MS") == 0) {
        set_numeric_uint32(&config->ws_backoff_max_ms, normalized);
    } else if (strcasecmp(key, "YUREI_QUEUE_CAPACITY") == 0) {
        set_numeric_size(&config->queue_capacity, normalized);
    } else if (strcasecmp(key, "YUREI_BATCH_SIZE") == 0) {
        set_numeric_uint32(&config->batch_size, normalized);
    } else if (strcasecmp(key, "YUREI_PUMPFUN_PROGRAM") == 0) {
        copy_string(config->pumpfun_program, sizeof(config->pumpfun_program), normalized);
    } else if (strcasecmp(key, "YUREI_RAYDIUM_PROGRAM") == 0) {
        copy_string(config->raydium_program, sizeof(config->raydium_program), normalized);
    } else if (strcasecmp(key, "YUREI_PUMPFUN_TABLE") == 0) {
        copy_string(config->pumpfun_table, sizeof(config->pumpfun_table), normalized);
    } else if (strcasecmp(key, "YUREI_RAYDIUM_TABLE") == 0) {
        copy_string(config->raydium_table, sizeof(config->raydium_table), normalized);
    } else if (strcasecmp(key, "YUREI_PG_CONN") == 0 ||
               strcasecmp(key, "YUREI_PG_CONNINFO") == 0) {
        copy_string(config->pg_conninfo, sizeof(config->pg_conninfo), normalized);
    } else if (strcasecmp(key, "YUREI_LOG_LEVEL") == 0) {
        copy_string(config->log_level, sizeof(config->log_level), normalized);
    } else if (strcasecmp(key, "YUREI_RPC_API_KEY") == 0) {
        copy_string(config->rpc_api_key, sizeof(config->rpc_api_key), normalized);
    } else if (strcasecmp(key, "YUREI_RATE_LIMIT") == 0) {
        set_numeric_uint32(&config->rate_limit_rps, normalized);
    } else if (strcasecmp(key, "YUREI_LOG_COLOR") == 0) {
        config->log_color = (strcasecmp(normalized, "1") == 0 ||
                             strcasecmp(normalized, "true") == 0 ||
                             strcasecmp(normalized, "yes") == 0);
    }
}

static void apply_environment_overrides(YureiConfig *config) {
    const char *keys[] = {
        "YUREI_RPC_ENDPOINT",
        "YUREI_WSS_ENDPOINT",
        "YUREI_RPC_API_KEY",
        "YUREI_RPC_MODE",
        "YUREI_POLL_INTERVAL_MS",
        "YUREI_WS_BACKOFF_MS",
        "YUREI_WS_BACKOFF_MAX_MS",
        "YUREI_QUEUE_CAPACITY",
        "YUREI_BATCH_SIZE",
        "YUREI_RATE_LIMIT",
        "YUREI_LOG_COLOR",
        "YUREI_PUMPFUN_PROGRAM",
        "YUREI_RAYDIUM_PROGRAM",
        "YUREI_PG_CONN",
        "YUREI_PG_CONNINFO",
        "YUREI_PUMPFUN_TABLE",
        "YUREI_RAYDIUM_TABLE",
        "YUREI_LOG_LEVEL"
    };

    size_t count = sizeof(keys) / sizeof(keys[0]);
    for (size_t i = 0; i < count; ++i) {
        const char *value = getenv(keys[i]);
        if (value && *value) {
            apply_key_value(config, keys[i], value);
        }
    }
}

static void load_env_file(const char *path, YureiConfig *config) {
    if (!path || !config) {
        return;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        return;
    }

    char line[512];
    char trimmed[512];
    while (fgets(line, sizeof(line), fp)) {
        const char *clean = trim(line, trimmed, sizeof(trimmed));
        if (!clean || clean[0] == '\0' || clean[0] == '#') {
            continue;
        }
        char *equals = strchr(trimmed, '=');
        if (!equals) {
            continue;
        }
        *equals = '\0';
        const char *key = trim(trimmed, trimmed, sizeof(trimmed));
        const char *value = trim(equals + 1, line, sizeof(line));
        apply_key_value(config, key, value);
    }

    fclose(fp);
}

static void configure_log_level(const YureiConfig *config) {
    if (!config) {
        return;
    }

    // Configure log colors
    yurei_log_set_color(config->log_color);

    // Configure log level
    if (strcasecmp(config->log_level, "trace") == 0) {
        yurei_log_set_level(YUREI_LOG_TRACE);
    } else if (strcasecmp(config->log_level, "debug") == 0) {
        yurei_log_set_level(YUREI_LOG_DEBUG);
    } else if (strcasecmp(config->log_level, "info") == 0) {
        yurei_log_set_level(YUREI_LOG_INFO);
    } else if (strcasecmp(config->log_level, "warn") == 0) {
        yurei_log_set_level(YUREI_LOG_WARN);
    } else {
        yurei_log_set_level(YUREI_LOG_ERROR);
    }
}

int yurei_config_load(const char *path, YureiConfig *config) {
    if (!config) {
        return -1;
    }
    apply_defaults(config);

    if (path) {
        load_env_file(path, config);
    } else {
        load_env_file(".env", config);
    }

    apply_environment_overrides(config);
    configure_log_level(config);
    yurei_config_print(config);
    return 0;
}

void yurei_config_print(const YureiConfig *config) {
    if (!config) {
        return;
    }
    YUREI_LOG_INFO("RPC endpoint: %s", config->rpc_endpoint);
    YUREI_LOG_INFO("WSS endpoint: %s", config->wss_endpoint);
    YUREI_LOG_INFO("Mode: %s", config->rpc_mode);
    YUREI_LOG_INFO("Poll interval: %u ms", config->poll_interval_ms);
    YUREI_LOG_INFO("Queue capacity: %zu", config->queue_capacity);
    YUREI_LOG_INFO("Batch size: %u", config->batch_size);
    YUREI_LOG_INFO("DB tables: pumpfun=%s raydium=%s",
                   config->pumpfun_table,
                   config->raydium_table);
}
