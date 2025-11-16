// Project Yurei - High-performance Solana data engine (MIT License)
// Copyright (c) 2025 Project Yurei
// https://x.com/yureiai  PRD: yurei-jsonrpc-client
#include "db_writer.h"

#include <libpq-fe.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "logging.h"

static const char *table_for_event(YureiEventKind kind, const YureiConfig *config) {
    if (!config) {
        return NULL;
    }
    switch (kind) {
        case YUREI_EVENT_KIND_PUMPFUN:
            return config->pumpfun_table[0] ? config->pumpfun_table : NULL;
        case YUREI_EVENT_KIND_RAYDIUM:
            return config->raydium_table[0] ? config->raydium_table : NULL;
        default:
            return NULL;
    }
}

static bool insert_event(PGconn *conn, const YureiEvent *event, const YureiConfig *config) {
    const char *table = table_for_event(event->kind, config);
    if (!table) {
        return true;
    }
    char query[256];
    snprintf(query,
             sizeof(query),
             "INSERT INTO %s (slot, signature, raw_log) VALUES ($1, $2, $3)"
             " ON CONFLICT DO NOTHING",
             table);

    char slot_buf[32];
    snprintf(slot_buf, sizeof(slot_buf), "%" PRIu64, event->slot);

    const char *paramValues[3] = {slot_buf, event->signature, (const char *)event->data};
    int paramLengths[3] = {(int)strlen(slot_buf), (int)strlen(event->signature),
                           (int)event->data_len};
    int paramFormats[3] = {0, 0, 1};

    PGresult *res = PQexecParams(conn,
                                 query,
                                 3,
                                 NULL,
                                 paramValues,
                                 paramLengths,
                                 paramFormats,
                                 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        YUREI_LOG_WARN("DB insert failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return false;
    }
    PQclear(res);
    return true;
}

static PGconn *wait_for_connection(const char *conninfo) {
    PGconn *conn = NULL;
    uint32_t backoff_ms = 1000;
    const uint32_t max_backoff = 30000;
    while (1) {
        conn = PQconnectdb(conninfo);
        if (PQstatus(conn) == CONNECTION_OK) {
            return conn;
        }
        YUREI_LOG_WARN("DB connection failed: %s", PQerrorMessage(conn));
        PQfinish(conn);
        conn = NULL;
        usleep(backoff_ms * 1000);
        if (backoff_ms < max_backoff) {
            backoff_ms *= 2;
            if (backoff_ms > max_backoff) {
                backoff_ms = max_backoff;
            }
        }
    }
}

static void *writer_thread(void *arg) {
    YureiDbWriter *writer = (YureiDbWriter *)arg;
    PGconn *conn = wait_for_connection(writer->config->pg_conninfo);
    YUREI_LOG_INFO("Connected to PostgreSQL");

    while (writer->running) {
        YureiEvent event;
        if (yurei_queue_pop(writer->queue, &event) != 0) {
            break;
        }
        if (!insert_event(conn, &event, writer->config)) {
            YUREI_LOG_WARN("Insert failed; reconnecting");
            PQfinish(conn);
            conn = wait_for_connection(writer->config->pg_conninfo);
        }
    }

    PQfinish(conn);
    return NULL;
}

int yurei_db_writer_start(YureiDbWriter *writer,
                          const YureiConfig *config,
                          YureiEventQueue *queue) {
    if (!writer || !config || !queue) {
        return -1;
    }
    memset(writer, 0, sizeof(*writer));
    writer->config = config;
    writer->queue = queue;
    writer->running = true;

    if (pthread_create(&writer->thread, NULL, writer_thread, writer) != 0) {
        writer->running = false;
        return -1;
    }
    return 0;
}

void yurei_db_writer_stop(YureiDbWriter *writer) {
    if (!writer) {
        return;
    }
    writer->running = false;
    pthread_join(writer->thread, NULL);
}
