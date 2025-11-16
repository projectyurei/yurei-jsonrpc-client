// Project Yurei - High-performance Solana data engine (MIT License)
// Copyright (c) 2025 Project Yurei
// https://x.com/yureiai  PRD: yurei-jsonrpc-client
#include "event_queue.h"

#include <stdlib.h>
#include <string.h>

int yurei_queue_init(YureiEventQueue *queue, size_t capacity) {
    if (!queue || capacity == 0) {
        return -1;
    }
    memset(queue, 0, sizeof(*queue));
    queue->buffer = calloc(capacity, sizeof(YureiEvent));
    if (!queue->buffer) {
        return -1;
    }
    queue->capacity = capacity;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond_push, NULL);
    pthread_cond_init(&queue->cond_pop, NULL);
    return 0;
}

void yurei_queue_destroy(YureiEventQueue *queue) {
    if (!queue) {
        return;
    }
    free(queue->buffer);
    queue->buffer = NULL;
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond_push);
    pthread_cond_destroy(&queue->cond_pop);
}

int yurei_queue_push(YureiEventQueue *queue, const YureiEvent *event) {
    if (!queue || !event) {
        return -1;
    }
    pthread_mutex_lock(&queue->mutex);
    while (queue->size == queue->capacity && !queue->closed) {
        pthread_cond_wait(&queue->cond_push, &queue->mutex);
    }
    if (queue->closed) {
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }
    queue->buffer[queue->tail] = *event;
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->size++;
    pthread_cond_signal(&queue->cond_pop);
    pthread_mutex_unlock(&queue->mutex);
    return 0;
}

int yurei_queue_pop(YureiEventQueue *queue, YureiEvent *event) {
    if (!queue || !event) {
        return -1;
    }
    pthread_mutex_lock(&queue->mutex);
    while (queue->size == 0 && !queue->closed) {
        pthread_cond_wait(&queue->cond_pop, &queue->mutex);
    }
    if (queue->size == 0 && queue->closed) {
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }
    *event = queue->buffer[queue->head];
    queue->head = (queue->head + 1) % queue->capacity;
    queue->size--;
    pthread_cond_signal(&queue->cond_push);
    pthread_mutex_unlock(&queue->mutex);
    return 0;
}

void yurei_queue_close(YureiEventQueue *queue) {
    if (!queue) {
        return;
    }
    pthread_mutex_lock(&queue->mutex);
    queue->closed = true;
    pthread_cond_broadcast(&queue->cond_pop);
    pthread_cond_broadcast(&queue->cond_push);
    pthread_mutex_unlock(&queue->mutex);
}
