// Project Yurei - High-performance Solana data engine
// Copyright 2025 Project Yurei. All rights reserved.
// https://x.com/yureiai

#ifndef YUREI_EVENT_QUEUE_H
#define YUREI_EVENT_QUEUE_H

#include <stdbool.h>
#include <stddef.h>

#include "yurei_event.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yurei_event_queue yurei_event_queue_t;

yurei_event_queue_t *event_queue_create(size_t capacity);
void event_queue_destroy(yurei_event_queue_t *queue);
bool event_queue_push(yurei_event_queue_t *queue, const yurei_event_t *event);
bool event_queue_pop(yurei_event_queue_t *queue, yurei_event_t *event, bool block);
void event_queue_close(yurei_event_queue_t *queue);
size_t event_queue_size(yurei_event_queue_t *queue);
size_t event_queue_capacity(yurei_event_queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif
