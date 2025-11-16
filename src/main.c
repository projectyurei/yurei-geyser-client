// Project Yurei - High-performance Solana data engine
// Copyright 2025 Project Yurei. All rights reserved.
// https://x.com/yureiai

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "db_writer.h"
#include "event_queue.h"
#include "geyser_client.h"
#include "log.h"
#include "protocol_detector.h"
#include "yurei_config.h"

static volatile sig_atomic_t g_stop = 0;

static void handle_signal(int signo) {
    (void)signo;
    g_stop = 1;
}

int main(void) {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    yurei_config_t config;
    if (!yurei_config_load(&config)) {
        fprintf(stderr, "failed to load configuration\n");
        return EXIT_FAILURE;
    }

    yurei_event_queue_t *queue = event_queue_create(config.queue_capacity);
    if (!queue) {
        fprintf(stderr, "failed to allocate event queue\n");
        return EXIT_FAILURE;
    }

    yurei_protocol_detector_t detector;
    protocol_detector_init(&detector,
                           config.pumpfun_program, config.pumpfun_enabled,
                           config.raydium_program, config.raydium_enabled);

    db_writer_params_t params = {
        .queue = queue,
        .config = &config,
    };
    db_writer_t *writer = db_writer_start(&params);
    if (!writer) {
        fprintf(stderr, "failed to start db writer\n");
        event_queue_destroy(queue);
        return EXIT_FAILURE;
    }

    geyser_client_t *client = geyser_client_start(&config, &detector, queue);
    if (!client) {
        fprintf(stderr, "failed to start geyser client\n");
        db_writer_stop(writer);
        event_queue_destroy(queue);
        return EXIT_FAILURE;
    }

    LOG_INFO("yurei geyser client started");
    while (!g_stop) {
        sleep(1);
    }

    LOG_INFO("shutting down");
    geyser_client_stop(client);
    db_writer_stop(writer);
    event_queue_destroy(queue);
    return EXIT_SUCCESS;
}
