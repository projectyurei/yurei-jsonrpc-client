// Project Yurei - High-performance Solana data engine (MIT License)
// Copyright (c) 2025 Project Yurei
// https://x.com/yureiai  PRD: yurei-jsonrpc-client
#ifndef YUREI_DB_WRITER_H
#define YUREI_DB_WRITER_H

#include <stdbool.h>
#include <pthread.h>

#include "config.h"
#include "event_queue.h"

typedef struct {
    bool running;
    pthread_t thread;
    const YureiConfig *config;
    YureiEventQueue *queue;
} YureiDbWriter;

int yurei_db_writer_start(YureiDbWriter *writer,
                          const YureiConfig *config,
                          YureiEventQueue *queue);
void yurei_db_writer_stop(YureiDbWriter *writer);

#endif // YUREI_DB_WRITER_H
