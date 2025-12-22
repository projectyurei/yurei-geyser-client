// Project Yurei - High-performance Solana data engine
// Copyright 2025 Project Yurei. All rights reserved.
// https://x.com/yureiai

#define _DEFAULT_SOURCE  // Enable usleep on glibc

#include "db_writer.h"

#include "base58.h"
#include "log.h"
#include "metrics.h"

#include <libpq-fe.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

// Batch configuration
#define BATCH_SIZE 100          // Max events per batch
#define FLUSH_INTERVAL_MS 50    // Max delay before flush (milliseconds)

struct db_writer {
    yurei_event_queue_t *queue;
    const yurei_config_t *config;
    PGconn *conn;
    pthread_t thread;
    bool running;
    
    // Batch buffers
    yurei_event_t pumpfun_batch[BATCH_SIZE];
    size_t pumpfun_count;
    yurei_event_t raydium_batch[BATCH_SIZE];
    size_t raydium_count;
    struct timeval last_flush;
};

static bool ensure_connection(struct db_writer *writer) {
    if (!writer->conn) {
        writer->conn = PQconnectdb(writer->config->db_url);
        if (PQstatus(writer->conn) == CONNECTION_OK) {
            metrics_inc_db_reconnect();
            LOG_INFO("Database connection established");
        }
    }
    if (PQstatus(writer->conn) != CONNECTION_OK) {
        LOG_ERROR("libpq: %s", PQerrorMessage(writer->conn));
        PQfinish(writer->conn);
        writer->conn = NULL;
        return false;
    }
    return true;
}

static uint64_t get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// Flush PumpFun batch using multi-row INSERT
static bool flush_pumpfun_batch(struct db_writer *writer) {
    if (writer->pumpfun_count == 0) return true;
    if (!ensure_connection(writer)) return false;

    uint64_t start_time = get_time_ms();
    
    // Build multi-row INSERT statement
    // Estimated max size: header + (count * row_size)
    size_t buf_size = 512 + writer->pumpfun_count * 600;
    char *query = malloc(buf_size);
    if (!query) return false;
    
    strcpy(query, "INSERT INTO pumpfun_trades (slot, tx_signature, mint, trader, creator, side, "
                  "sol_amount, token_amount, fee_bps, fee_lamports, creator_fee_bps, creator_fee_lamports, "
                  "virtual_sol_reserves, virtual_token_reserves, real_sol_reserves, real_token_reserves) VALUES ");
    
    size_t offset = strlen(query);
    
    for (size_t i = 0; i < writer->pumpfun_count; i++) {
        const yurei_event_t *event = &writer->pumpfun_batch[i];
        
        char mint_b58[64], trader_b58[64], creator_b58[64];
        if (base58_encode(event->data.pumpfun_trade.mint, 32, mint_b58, sizeof(mint_b58)) < 0 ||
            base58_encode(event->data.pumpfun_trade.trader, 32, trader_b58, sizeof(trader_b58)) < 0 ||
            base58_encode(event->data.pumpfun_trade.creator, 32, creator_b58, sizeof(creator_b58)) < 0) {
            continue;
        }
        
        if (i > 0) {
            offset += snprintf(query + offset, buf_size - offset, ",");
        }
        
        offset += snprintf(query + offset, buf_size - offset,
            "(%lu,'%s','%s','%s','%s','%s',%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu)",
            event->data.pumpfun_trade.slot,
            event->signature,
            mint_b58, trader_b58, creator_b58,
            event->data.pumpfun_trade.is_buy ? "BUY" : "SELL",
            event->data.pumpfun_trade.sol_amount,
            event->data.pumpfun_trade.token_amount,
            event->data.pumpfun_trade.fee_basis_points,
            event->data.pumpfun_trade.fee_lamports,
            event->data.pumpfun_trade.creator_fee_basis_points,
            event->data.pumpfun_trade.creator_fee_lamports,
            event->data.pumpfun_trade.virtual_sol_reserves,
            event->data.pumpfun_trade.virtual_token_reserves,
            event->data.pumpfun_trade.real_sol_reserves,
            event->data.pumpfun_trade.real_token_reserves);
    }
    
    PGresult *res = PQexec(writer->conn, query);
    free(query);
    
    uint64_t latency = get_time_ms() - start_time;
    metrics_add_db_latency(latency * 1000);  // Convert to microseconds
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        LOG_ERROR("pumpfun batch insert failed: %s", PQerrorMessage(writer->conn));
        PQclear(res);
        metrics_inc_db_failed();
        return false;
    }
    
    PQclear(res);
    
    for (size_t i = 0; i < writer->pumpfun_count; i++) {
        metrics_inc_db_success();
        metrics_inc_pumpfun();
    }
    metrics_inc_db_batch();
    
    LOG_DEBUG("Flushed %zu PumpFun events in %lu ms", writer->pumpfun_count, latency);
    writer->pumpfun_count = 0;
    return true;
}

// Flush Raydium batch using multi-row INSERT
static bool flush_raydium_batch(struct db_writer *writer) {
    if (writer->raydium_count == 0) return true;
    if (!ensure_connection(writer)) return false;

    uint64_t start_time = get_time_ms();
    
    size_t buf_size = 256 + writer->raydium_count * 300;
    char *query = malloc(buf_size);
    if (!query) return false;
    
    strcpy(query, "INSERT INTO raydium_swaps (slot, tx_signature, pool, user_owner, amount_in, amount_out) VALUES ");
    
    size_t offset = strlen(query);
    
    for (size_t i = 0; i < writer->raydium_count; i++) {
        const yurei_event_t *event = &writer->raydium_batch[i];
        
        char amm_b58[64], owner_b58[64];
        if (base58_encode(event->data.raydium_swap.amm, 32, amm_b58, sizeof(amm_b58)) < 0 ||
            base58_encode(event->data.raydium_swap.user_source_owner, 32, owner_b58, sizeof(owner_b58)) < 0) {
            continue;
        }
        
        if (i > 0) {
            offset += snprintf(query + offset, buf_size - offset, ",");
        }
        
        offset += snprintf(query + offset, buf_size - offset,
            "(%lu,'%s','%s','%s',%lu,%lu)",
            event->data.raydium_swap.slot,
            event->signature,
            amm_b58, owner_b58,
            event->data.raydium_swap.amount_in,
            event->data.raydium_swap.amount_out);
    }
    
    PGresult *res = PQexec(writer->conn, query);
    free(query);
    
    uint64_t latency = get_time_ms() - start_time;
    metrics_add_db_latency(latency * 1000);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        LOG_ERROR("raydium batch insert failed: %s", PQerrorMessage(writer->conn));
        PQclear(res);
        metrics_inc_db_failed();
        return false;
    }
    
    PQclear(res);
    
    for (size_t i = 0; i < writer->raydium_count; i++) {
        metrics_inc_db_success();
        metrics_inc_raydium();
    }
    metrics_inc_db_batch();
    
    LOG_DEBUG("Flushed %zu Raydium events in %lu ms", writer->raydium_count, latency);
    writer->raydium_count = 0;
    return true;
}

static void flush_all_batches(struct db_writer *writer) {
    flush_pumpfun_batch(writer);
    flush_raydium_batch(writer);
    gettimeofday(&writer->last_flush, NULL);
}

static bool should_flush_timer(struct db_writer *writer) {
    struct timeval now;
    gettimeofday(&now, NULL);
    uint64_t elapsed_ms = (now.tv_sec - writer->last_flush.tv_sec) * 1000 +
                          (now.tv_usec - writer->last_flush.tv_usec) / 1000;
    return elapsed_ms >= FLUSH_INTERVAL_MS;
}

static void *db_writer_main(void *arg) {
    struct db_writer *writer = arg;
    gettimeofday(&writer->last_flush, NULL);
    
    while (writer->running) {
        yurei_event_t event;
        
        // Non-blocking pop to allow timer-based flushing
        if (!event_queue_pop(writer->queue, &event, false)) {
            // No event available - check flush timer
            if (should_flush_timer(writer)) {
                flush_all_batches(writer);
            }
            usleep(1000);  // 1ms sleep to avoid busy-wait
            continue;
        }
        
        metrics_inc_events_total();
        
        switch (event.type) {
        case YUREI_EVENT_PUMPFUN_TRADE:
            writer->pumpfun_batch[writer->pumpfun_count++] = event;
            if (writer->pumpfun_count >= BATCH_SIZE) {
                flush_pumpfun_batch(writer);
            }
            break;
        case YUREI_EVENT_RAYDIUM_SWAP:
            writer->raydium_batch[writer->raydium_count++] = event;
            if (writer->raydium_count >= BATCH_SIZE) {
                flush_raydium_batch(writer);
            }
            break;
        default:
            break;
        }
        
        // Timer-based flush for low-volume periods
        if (should_flush_timer(writer)) {
            flush_all_batches(writer);
        }
    }
    
    // Final flush on shutdown
    flush_all_batches(writer);
    
    if (writer->conn) {
        PQfinish(writer->conn);
        writer->conn = NULL;
    }
    return NULL;
}

db_writer_t *db_writer_start(const db_writer_params_t *params) {
    if (!params)
        return NULL;
    struct db_writer *writer = calloc(1, sizeof(*writer));
    if (!writer)
        return NULL;
    writer->queue = params->queue;
    writer->config = params->config;
    writer->running = true;
    writer->pumpfun_count = 0;
    writer->raydium_count = 0;
    
    if (pthread_create(&writer->thread, NULL, db_writer_main, writer) != 0) {
        free(writer);
        return NULL;
    }
    
    LOG_INFO("DB writer started (batch_size=%d, flush_interval=%dms)", BATCH_SIZE, FLUSH_INTERVAL_MS);
    return writer;
}

void db_writer_stop(db_writer_t *writer) {
    if (!writer)
        return;
    writer->running = false;
    event_queue_close(writer->queue);
    pthread_join(writer->thread, NULL);
    free(writer);
    LOG_INFO("DB writer stopped");
}

