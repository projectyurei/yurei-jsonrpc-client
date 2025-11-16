// Project Yurei - High-performance Solana data engine (MIT License)
// Copyright (c) 2025 Project Yurei
// https://x.com/yureiai  PRD: yurei-jsonrpc-client
#ifndef YUREI_EVENT_QUEUE_H
#define YUREI_EVENT_QUEUE_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define YUREI_EVENT_PAYLOAD_MAX 4096

typedef enum {
    YUREI_EVENT_KIND_UNKNOWN = 0,
    YUREI_EVENT_KIND_PUMPFUN,
    YUREI_EVENT_KIND_RAYDIUM
} YureiEventKind;

typedef struct {
    YureiEventKind kind;
    char program_id[64];
    char signature[128];
    uint64_t slot;
    uint8_t data[YUREI_EVENT_PAYLOAD_MAX];
    size_t data_len;
} YureiEvent;

typedef struct {
    YureiEvent *buffer;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t size;
    bool closed;
    pthread_mutex_t mutex;
    pthread_cond_t cond_push;
    pthread_cond_t cond_pop;
} YureiEventQueue;

int yurei_queue_init(YureiEventQueue *queue, size_t capacity);
void yurei_queue_destroy(YureiEventQueue *queue);
int yurei_queue_push(YureiEventQueue *queue, const YureiEvent *event);
int yurei_queue_pop(YureiEventQueue *queue, YureiEvent *event);
void yurei_queue_close(YureiEventQueue *queue);

#endif // YUREI_EVENT_QUEUE_H
