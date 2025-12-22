// Project Yurei - High-performance Solana data engine
// Copyright 2025 Project Yurei. All rights reserved.
// https://x.com/yureiai

#include "event_queue.h"
#include "metrics.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

struct yurei_event_queue {
    yurei_event_t *buffer;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t size;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    bool closed;
};

yurei_event_queue_t *event_queue_create(size_t capacity) {
    yurei_event_queue_t *queue = calloc(1, sizeof(*queue));
    if (!queue)
        return NULL;
    queue->buffer = calloc(capacity, sizeof(yurei_event_t));
    if (!queue->buffer) {
        free(queue);
        return NULL;
    }
    queue->capacity = capacity;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);
    return queue;
}

void event_queue_destroy(yurei_event_queue_t *queue) {
    if (!queue)
        return;
    pthread_mutex_destroy(&queue->lock);
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
    free(queue->buffer);
    free(queue);
}

bool event_queue_push(yurei_event_queue_t *queue, const yurei_event_t *event) {
    pthread_mutex_lock(&queue->lock);
    while (!queue->closed && queue->size == queue->capacity) {
        pthread_cond_wait(&queue->not_full, &queue->lock);
    }
    if (queue->closed) {
        pthread_mutex_unlock(&queue->lock);
        return false;
    }
    queue->buffer[queue->tail] = *event;
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->size++;
    
    // Update metrics
    metrics_inc_queue_push();
    metrics_update_queue_high_water(queue->size);
    
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->lock);
    return true;
}

bool event_queue_pop(yurei_event_queue_t *queue, yurei_event_t *event, bool block) {
    pthread_mutex_lock(&queue->lock);
    while (!queue->closed && queue->size == 0) {
        if (!block) {
            pthread_mutex_unlock(&queue->lock);
            return false;
        }
        pthread_cond_wait(&queue->not_empty, &queue->lock);
    }
    if (queue->size == 0) {
        pthread_mutex_unlock(&queue->lock);
        return false;
    }
    *event = queue->buffer[queue->head];
    queue->head = (queue->head + 1) % queue->capacity;
    queue->size--;
    
    // Update metrics
    metrics_inc_queue_pop();
    
    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->lock);
    return true;
}

void event_queue_close(yurei_event_queue_t *queue) {
    pthread_mutex_lock(&queue->lock);
    queue->closed = true;
    pthread_cond_broadcast(&queue->not_empty);
    pthread_cond_broadcast(&queue->not_full);
    pthread_mutex_unlock(&queue->lock);
}

size_t event_queue_size(yurei_event_queue_t *queue) {
    pthread_mutex_lock(&queue->lock);
    size_t sz = queue->size;
    pthread_mutex_unlock(&queue->lock);
    return sz;
}

size_t event_queue_capacity(yurei_event_queue_t *queue) {
    return queue->capacity;
}

