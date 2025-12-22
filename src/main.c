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
#include "metrics.h"
#include "protocol_detector.h"
#include "yurei_config.h"

#define YUREI_VERSION "1.1.0"
#define METRICS_LOG_INTERVAL 60  // seconds

static volatile sig_atomic_t g_stop = 0;

static void handle_signal(int signo) {
    (void)signo;
    g_stop = 1;
}

int main(void) {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Initialize logging and metrics first
    yurei_log_init();
    metrics_init();

    LOG_INFO("=================================================");
    LOG_INFO("  YUREI GEYSER CLIENT v%s", YUREI_VERSION);
    LOG_INFO("  High-performance Solana data engine");
    LOG_INFO("  https://x.com/yureiai");
    LOG_INFO("=================================================");

    yurei_config_t config;
    if (!yurei_config_load(&config)) {
        LOG_ERROR("failed to load configuration");
        return EXIT_FAILURE;
    }

    LOG_INFO("Endpoint: %s", config.endpoint);
    LOG_INFO("Queue capacity: %zu", config.queue_capacity);

    yurei_event_queue_t *queue = event_queue_create(config.queue_capacity);
    if (!queue) {
        LOG_ERROR("failed to allocate event queue");
        return EXIT_FAILURE;
    }

    yurei_protocol_detector_t detector;
    protocol_detector_init(&detector,
                           config.pumpfun_program, config.pumpfun_enabled,
                           config.raydium_program, config.raydium_enabled);

    if (config.pumpfun_enabled) LOG_INFO("PumpFun detection: ENABLED");
    if (config.raydium_enabled) LOG_INFO("Raydium detection: ENABLED");

    db_writer_params_t params = {
        .queue = queue,
        .config = &config,
    };
    db_writer_t *writer = db_writer_start(&params);
    if (!writer) {
        LOG_ERROR("failed to start db writer");
        event_queue_destroy(queue);
        return EXIT_FAILURE;
    }

    geyser_client_t *client = geyser_client_start(&config, &detector, queue);
    if (!client) {
        LOG_ERROR("failed to start geyser client");
        db_writer_stop(writer);
        event_queue_destroy(queue);
        return EXIT_FAILURE;
    }

    LOG_INFO("yurei geyser client started - listening for events...");
    
    time_t last_metrics_log = time(NULL);
    while (!g_stop) {
        sleep(1);
        
        // Periodic metrics logging
        time_t now = time(NULL);
        if (now - last_metrics_log >= METRICS_LOG_INTERVAL) {
            metrics_log_summary();
            last_metrics_log = now;
        }
    }

    LOG_INFO("shutting down...");
    geyser_client_stop(client);
    db_writer_stop(writer);
    event_queue_destroy(queue);
    
    // Final metrics dump
    LOG_INFO("Final statistics:");
    metrics_log_summary();
    
    return EXIT_SUCCESS;
}

