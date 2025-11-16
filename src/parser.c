// Project Yurei - High-performance Solana data engine (MIT License)
// Copyright (c) 2025 Project Yurei
// https://x.com/yureiai  PRD: yurei-jsonrpc-client
#include "parser.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#ifdef __has_include
#if __has_include(<cjson/cJSON.h>)
#include <cjson/cJSON.h>
#else
#include <cJSON.h>
#endif
#else
#include <cjson/cJSON.h>
#endif

#include "logging.h"

typedef struct {
    const YureiConfig *config;
    YureiEventQueue *queue;
    uint64_t highest_slot;
} ParserContext;

static YureiEventKind program_to_kind(const char *program_id, const YureiConfig *config) {
    if (!program_id || !config) {
        return YUREI_EVENT_KIND_UNKNOWN;
    }
    if (config->pumpfun_program[0] &&
        strcasecmp(program_id, config->pumpfun_program) == 0) {
        return YUREI_EVENT_KIND_PUMPFUN;
    }
    if (config->raydium_program[0] &&
        strcasecmp(program_id, config->raydium_program) == 0) {
        return YUREI_EVENT_KIND_RAYDIUM;
    }
    return YUREI_EVENT_KIND_UNKNOWN;
}

static int8_t base64_table[256];
static bool base64_table_initialized = false;

static void init_base64_table(void) {
    if (base64_table_initialized) {
        return;
    }
    for (int i = 0; i < 256; ++i) {
        base64_table[i] = -1;
    }
    const char *alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int i = 0; alphabet[i]; ++i) {
        base64_table[(unsigned char)alphabet[i]] = (int8_t)i;
    }
    base64_table[(unsigned char)'='] = 0;
    base64_table_initialized = true;
}

static bool decode_base64(const char *input, uint8_t *output, size_t *out_len) {
    init_base64_table();
    if (!input || !output || !out_len) {
        return false;
    }

    size_t out_index = 0;
    int val = 0;
    int bits = -8;
    for (const unsigned char *ptr = (const unsigned char *)input; *ptr; ++ptr) {
        if (isspace(*ptr)) {
            continue;
        }
        int8_t decoded = base64_table[*ptr];
        if (decoded == -1) {
            break;
        }
        val = (val << 6) + decoded;
        bits += 6;
        if (bits >= 0) {
            if (out_index >= YUREI_EVENT_PAYLOAD_MAX) {
                return false;
            }
            output[out_index++] = (uint8_t)((val >> bits) & 0xFF);
            bits -= 8;
        }
    }
    *out_len = out_index;
    return true;
}

static void enqueue_event(ParserContext *ctx,
                          const char *program_id,
                          const char *signature,
                          uint64_t slot,
                          const char *line,
                          int *event_count) {
    if (!ctx || !line) {
        return;
    }

    const char *marker = strstr(line, "Program data:");
    if (!marker) {
        return;
    }
    marker += strlen("Program data:");
    while (*marker && isspace((unsigned char)*marker)) {
        marker++;
    }
    if (!*marker) {
        return;
    }

    YureiEvent event;
    memset(&event, 0, sizeof(event));
    event.slot = slot;
    if (signature) {
        snprintf(event.signature, sizeof(event.signature), "%s", signature);
    }
    if (program_id) {
        snprintf(event.program_id, sizeof(event.program_id), "%s", program_id);
    }
    event.kind = program_to_kind(program_id ? program_id : ctx->config->pumpfun_program,
                                 ctx->config);

    if (!decode_base64(marker, event.data, &event.data_len)) {
        YUREI_LOG_WARN("Failed to decode base64 payload (signature=%s)",
                       event.signature);
        return;
    }

    if (yurei_queue_push(ctx->queue, &event) == 0) {
        if (event_count) {
            (*event_count)++;
        }
        if (slot > ctx->highest_slot) {
            ctx->highest_slot = slot;
        }
    } else {
        YUREI_LOG_WARN("Queue backpressure prevented enqueue of slot=%" PRIu64, slot);
    }
}

static void process_logs_array(cJSON *logs,
                               const char *program_id,
                               const char *signature,
                               uint64_t slot,
                               ParserContext *ctx,
                               int *event_count) {
    if (!cJSON_IsArray(logs)) {
        return;
    }
    cJSON *log = NULL;
    cJSON_ArrayForEach(log, logs) {
        if (!cJSON_IsString(log) || !log->valuestring) {
            continue;
        }
        enqueue_event(ctx, program_id, signature, slot, log->valuestring, event_count);
    }
}

static void process_value_object(cJSON *value,
                                 ParserContext *ctx,
                                 int *event_count,
                                 uint64_t fallback_slot) {
    if (!cJSON_IsObject(value)) {
        return;
    }

    cJSON *logs = cJSON_GetObjectItemCaseSensitive(value, "logs");
    cJSON *signature = cJSON_GetObjectItemCaseSensitive(value, "signature");
    cJSON *slot_item = cJSON_GetObjectItemCaseSensitive(value, "slot");
    cJSON *program = cJSON_GetObjectItemCaseSensitive(value, "programId");

    uint64_t slot = fallback_slot;
    if (cJSON_IsNumber(slot_item)) {
        slot = (uint64_t)slot_item->valuedouble;
    }

    const char *sig_str = cJSON_IsString(signature) ? signature->valuestring : NULL;
    const char *program_id = cJSON_IsString(program) ? program->valuestring : NULL;

    process_logs_array(logs, program_id, sig_str, slot, ctx, event_count);
}

static void process_result_object(cJSON *result, ParserContext *ctx, int *event_count) {
    cJSON *value = cJSON_GetObjectItemCaseSensitive(result, "value");
    cJSON *context = cJSON_GetObjectItemCaseSensitive(result, "context");
    uint64_t slot = 0;
    if (cJSON_IsObject(context)) {
        cJSON *slot_item = cJSON_GetObjectItemCaseSensitive(context, "slot");
        if (cJSON_IsNumber(slot_item)) {
            slot = (uint64_t)slot_item->valuedouble;
        }
    }

    if (cJSON_IsObject(value)) {
        cJSON *inner_slot = cJSON_GetObjectItemCaseSensitive(value, "slot");
        if (cJSON_IsNumber(inner_slot)) {
            slot = (uint64_t)inner_slot->valuedouble;
        }
        process_value_object(value, ctx, event_count, slot);
    } else if (cJSON_IsArray(value)) {
        cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, value) {
            process_value_object(entry, ctx, event_count, slot);
        }
    }
}

int yurei_parser_handle_message(const char *json,
                                const YureiConfig *config,
                                YureiEventQueue *queue,
                                uint64_t *out_highest_slot) {
    if (!json || !config || !queue) {
        return -1;
    }

    ParserContext ctx = {
        .config = config,
        .queue = queue,
        .highest_slot = out_highest_slot && *out_highest_slot ? *out_highest_slot : 0
    };

    int event_count = 0;
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        YUREI_LOG_WARN("Failed to parse JSON payload");
        return -1;
    }

    cJSON *result = cJSON_GetObjectItemCaseSensitive(root, "result");
    cJSON *params = cJSON_GetObjectItemCaseSensitive(root, "params");

    if (cJSON_IsObject(result)) {
        process_result_object(result, &ctx, &event_count);
    } else if (cJSON_IsArray(result)) {
        cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, result) {
            process_value_object(entry, &ctx, &event_count, 0);
        }
    } else if (cJSON_IsObject(params)) {
        cJSON *params_result = cJSON_GetObjectItemCaseSensitive(params, "result");
        if (cJSON_IsObject(params_result)) {
            process_result_object(params_result, &ctx, &event_count);
        }
    }

    if (out_highest_slot) {
        *out_highest_slot = ctx.highest_slot;
    }

    cJSON_Delete(root);
    return event_count;
}
