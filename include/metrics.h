// Project Yurei - High-performance Solana data engine
// Copyright 2025 Project Yurei. All rights reserved.
// https://x.com/yureiai

#ifndef YUREI_METRICS_H
#define YUREI_METRICS_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Metrics collection for YUREI Agent performance monitoring
typedef struct {
    // Event counters
    _Atomic uint64_t events_total;
    _Atomic uint64_t events_pumpfun;
    _Atomic uint64_t events_raydium;
    _Atomic uint64_t events_dropped;

    // Queue stats
    _Atomic uint64_t queue_pushes;
    _Atomic uint64_t queue_pops;
    _Atomic uint64_t queue_high_water;
    _Atomic uint64_t queue_overflows;

    // Database stats
    _Atomic uint64_t db_inserts_success;
    _Atomic uint64_t db_inserts_failed;
    _Atomic uint64_t db_batches;
    _Atomic uint64_t db_reconnects;

    // Timing (microseconds)
    _Atomic uint64_t total_event_latency_us;
    _Atomic uint64_t total_db_latency_us;

    // Start time for uptime calculation
    struct timespec start_time;
} yurei_metrics_t;

// Global metrics instance
extern yurei_metrics_t g_metrics;

// Initialize metrics
void metrics_init(void);

// Increment helpers (thread-safe)
static inline void metrics_inc_events_total(void) {
    atomic_fetch_add(&g_metrics.events_total, 1);
}

static inline void metrics_inc_pumpfun(void) {
    atomic_fetch_add(&g_metrics.events_pumpfun, 1);
}

static inline void metrics_inc_raydium(void) {
    atomic_fetch_add(&g_metrics.events_raydium, 1);
}

static inline void metrics_inc_dropped(void) {
    atomic_fetch_add(&g_metrics.events_dropped, 1);
}

static inline void metrics_inc_queue_push(void) {
    atomic_fetch_add(&g_metrics.queue_pushes, 1);
}

static inline void metrics_inc_queue_pop(void) {
    atomic_fetch_add(&g_metrics.queue_pops, 1);
}

static inline void metrics_update_queue_high_water(uint64_t size) {
    uint64_t current = atomic_load(&g_metrics.queue_high_water);
    while (size > current) {
        if (atomic_compare_exchange_weak(&g_metrics.queue_high_water, &current, size))
            break;
    }
}

static inline void metrics_inc_queue_overflow(void) {
    atomic_fetch_add(&g_metrics.queue_overflows, 1);
}

static inline void metrics_inc_db_success(void) {
    atomic_fetch_add(&g_metrics.db_inserts_success, 1);
}

static inline void metrics_inc_db_failed(void) {
    atomic_fetch_add(&g_metrics.db_inserts_failed, 1);
}

static inline void metrics_inc_db_batch(void) {
    atomic_fetch_add(&g_metrics.db_batches, 1);
}

static inline void metrics_inc_db_reconnect(void) {
    atomic_fetch_add(&g_metrics.db_reconnects, 1);
}

static inline void metrics_add_event_latency(uint64_t us) {
    atomic_fetch_add(&g_metrics.total_event_latency_us, us);
}

static inline void metrics_add_db_latency(uint64_t us) {
    atomic_fetch_add(&g_metrics.total_db_latency_us, us);
}

// Calculate uptime in seconds
double metrics_uptime_seconds(void);

// Log a summary of all metrics
void metrics_log_summary(void);

// Get current metrics snapshot (for external monitoring)
typedef struct {
    uint64_t events_total;
    uint64_t events_pumpfun;
    uint64_t events_raydium;
    uint64_t events_dropped;
    uint64_t queue_high_water;
    uint64_t db_inserts_success;
    uint64_t db_inserts_failed;
    double uptime_seconds;
    double events_per_second;
    double avg_event_latency_us;
    double avg_db_latency_us;
} yurei_metrics_snapshot_t;

void metrics_snapshot(yurei_metrics_snapshot_t *out);

#ifdef __cplusplus
}
#endif

#endif
