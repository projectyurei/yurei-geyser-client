// Project Yurei - High-performance Solana data engine
// Copyright 2025 Project Yurei. All rights reserved.
// https://x.com/yureiai

#ifndef YUREI_DB_WRITER_H
#define YUREI_DB_WRITER_H

#include <stdbool.h>

#include "event_queue.h"
#include "yurei_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    yurei_event_queue_t *queue;
    const yurei_config_t *config;
} db_writer_params_t;

typedef struct db_writer db_writer_t;

db_writer_t *db_writer_start(const db_writer_params_t *params);
void db_writer_stop(db_writer_t *writer);

#ifdef __cplusplus
}
#endif

#endif
