// Project Yurei - High-performance Solana data engine
// Copyright 2025 Project Yurei. All rights reserved.
// https://x.com/yureiai

#ifndef YUREI_GEYSER_CLIENT_H
#define YUREI_GEYSER_CLIENT_H

#include <stdbool.h>

#include "event_queue.h"
#include "protocol_detector.h"
#include "yurei_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct geyser_client geyser_client_t;

geyser_client_t *geyser_client_start(const yurei_config_t *config,
                                     const yurei_protocol_detector_t *detector,
                                     yurei_event_queue_t *queue);
void geyser_client_stop(geyser_client_t *client);

#ifdef __cplusplus
}
#endif

#endif
