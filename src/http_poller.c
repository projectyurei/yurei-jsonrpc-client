// Project Yurei - High-performance Solana data engine (MIT License)
// Copyright (c) 2025 Project Yurei
// https://x.com/yureiai  PRD: yurei-jsonrpc-client
#include "http_poller.h"

#include <curl/curl.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "logging.h"
#include "parser.h"

typedef struct {
    char *data;
    size_t length;
} CurlBuffer;

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    CurlBuffer *buffer = (CurlBuffer *)userdata;
    char *new_data = realloc(buffer->data, buffer->length + total + 1);
    if (!new_data) {
        return 0;
    }
    buffer->data = new_data;
    memcpy(buffer->data + buffer->length, ptr, total);
    buffer->length += total;
    buffer->data[buffer->length] = '\0';
    return total;
}

static void build_mentions_array(const YureiConfig *config, char *out, size_t len) {
    bool first = true;
    size_t offset = 0;
    if (len == 0) {
        return;
    }
    offset += snprintf(out + offset, len - offset, "[");
    if (config->pumpfun_program[0]) {
        offset += snprintf(out + offset,
                           len - offset,
                           "%s\"%s\"",
                           first ? "" : ",",
                           config->pumpfun_program);
        first = false;
    }
    if (config->raydium_program[0]) {
        offset += snprintf(out + offset,
                           len - offset,
                           "%s\"%s\"",
                           first ? "" : ",",
                           config->raydium_program);
        first = false;
    }
    if (first) {
        offset += snprintf(out + offset,
                           len - offset,
                           "\"%s\"",
                           config->pumpfun_program);
    }
    snprintf(out + offset, len - offset, "]");
}

static void *poller_thread(void *arg) {
    YureiHttpPoller *poller = (YureiHttpPoller *)arg;
    CURL *curl = curl_easy_init();
    if (!curl) {
        YUREI_LOG_ERROR("Failed to initialize libcurl");
        poller->running = false;
        return NULL;
    }

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, poller->config->rpc_endpoint);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    while (poller->running) {
        char payload[512];
        char mentions[256];
        build_mentions_array(poller->config, mentions, sizeof(mentions));

        if (poller->last_slot > 0) {
            snprintf(payload,
                     sizeof(payload),
                     "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"getLogs\","
                     "\"params\":[{\"mentions\":%s,\"startSlot\":%" PRIu64 ",\"limit\":50},"
                     "{\"commitment\":\"confirmed\"}]}",
                     mentions,
                     poller->last_slot);
        } else {
            snprintf(payload,
                     sizeof(payload),
                     "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"getLogs\","
                     "\"params\":[{\"mentions\":%s,\"limit\":50},{\"commitment\":\"confirmed\"}]}",
                     mentions);
        }

        CurlBuffer buffer = {.data = NULL, .length = 0};
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

        CURLcode rc = curl_easy_perform(curl);
        if (rc == CURLE_OK && buffer.data) {
            uint64_t highest_slot = poller->last_slot;
            int processed = yurei_parser_handle_message(
                buffer.data, poller->config, poller->queue, &highest_slot);
            if (processed > 0 && highest_slot > poller->last_slot) {
                poller->last_slot = highest_slot;
            }
        } else {
            YUREI_LOG_WARN("HTTP poll failed: %s", curl_easy_strerror(rc));
        }

        free(buffer.data);
        usleep(poller->config->poll_interval_ms * 1000);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return NULL;
}

int yurei_http_poller_start(YureiHttpPoller *poller,
                            const YureiConfig *config,
                            YureiEventQueue *queue) {
    if (!poller || !config || !queue) {
        return -1;
    }
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
        YUREI_LOG_ERROR("curl_global_init failed");
        return -1;
    }
    memset(poller, 0, sizeof(*poller));
    poller->config = config;
    poller->queue = queue;
    poller->running = true;

    if (pthread_create(&poller->thread, NULL, poller_thread, poller) != 0) {
        poller->running = false;
        return -1;
    }
    return 0;
}

void yurei_http_poller_stop(YureiHttpPoller *poller) {
    if (!poller) {
        return;
    }
    poller->running = false;
    pthread_join(poller->thread, NULL);
    curl_global_cleanup();
}
