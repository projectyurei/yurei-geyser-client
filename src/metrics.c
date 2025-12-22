// Project Yurei - High-performance Solana data engine
// Copyright 2025 Project Yurei. All rights reserved.
// https://x.com/yureiai

#include "metrics.h"
#include "log.h"

#include <string.h>

// Global metrics instance
yurei_metrics_t g_metrics = {0};

void metrics_init(void) {
    memset(&g_metrics, 0, sizeof(g_metrics));
    clock_gettime(CLOCK_MONOTONIC, &g_metrics.start_time);
}

double metrics_uptime_seconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    double elapsed = (now.tv_sec - g_metrics.start_time.tv_sec) +
                     (now.tv_nsec - g_metrics.start_time.tv_nsec) / 1e9;
    return elapsed > 0 ? elapsed : 0.001;
}

void metrics_snapshot(yurei_metrics_snapshot_t *out) {
    if (!out) return;
    
    out->events_total = atomic_load(&g_metrics.events_total);
    out->events_pumpfun = atomic_load(&g_metrics.events_pumpfun);
    out->events_raydium = atomic_load(&g_metrics.events_raydium);
    out->events_dropped = atomic_load(&g_metrics.events_dropped);
    out->queue_high_water = atomic_load(&g_metrics.queue_high_water);
    out->db_inserts_success = atomic_load(&g_metrics.db_inserts_success);
    out->db_inserts_failed = atomic_load(&g_metrics.db_inserts_failed);
    out->uptime_seconds = metrics_uptime_seconds();
    
    // Calculate rates
    out->events_per_second = out->uptime_seconds > 0 
        ? (double)out->events_total / out->uptime_seconds 
        : 0;
    
    uint64_t total_event_latency = atomic_load(&g_metrics.total_event_latency_us);
    out->avg_event_latency_us = out->events_total > 0 
        ? (double)total_event_latency / out->events_total 
        : 0;
    
    uint64_t total_db_latency = atomic_load(&g_metrics.total_db_latency_us);
    out->avg_db_latency_us = out->db_inserts_success > 0 
        ? (double)total_db_latency / out->db_inserts_success 
        : 0;
}

void metrics_log_summary(void) {
    yurei_metrics_snapshot_t snap;
    metrics_snapshot(&snap);
    
    LOG_INFO("=== YUREI METRICS ===");
    LOG_INFO("  Uptime: %.2f sec | Events/sec: %.2f", 
             snap.uptime_seconds, snap.events_per_second);
    LOG_INFO("  Events: total=%lu pumpfun=%lu raydium=%lu dropped=%lu",
             snap.events_total, snap.events_pumpfun, snap.events_raydium, snap.events_dropped);
    LOG_INFO("  DB: success=%lu failed=%lu batches=%lu reconnects=%lu",
             snap.db_inserts_success, snap.db_inserts_failed,
             atomic_load(&g_metrics.db_batches),
             atomic_load(&g_metrics.db_reconnects));
    LOG_INFO("  Queue: pushes=%lu pops=%lu high_water=%lu overflows=%lu",
             atomic_load(&g_metrics.queue_pushes),
             atomic_load(&g_metrics.queue_pops),
             snap.queue_high_water,
             atomic_load(&g_metrics.queue_overflows));
    LOG_INFO("  Latency: event_avg=%.2fus db_avg=%.2fus",
             snap.avg_event_latency_us, snap.avg_db_latency_us);
    LOG_INFO("=====================");
}
