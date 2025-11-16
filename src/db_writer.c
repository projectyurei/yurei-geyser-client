// Project Yurei - High-performance Solana data engine
// Copyright 2025 Project Yurei. All rights reserved.
// https://x.com/yureiai

#include "db_writer.h"

#include "base58.h"
#include "log.h"

#include <libpq-fe.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct db_writer {
    yurei_event_queue_t *queue;
    const yurei_config_t *config;
    PGconn *conn;
    pthread_t thread;
    bool running;
};

static bool ensure_connection(struct db_writer *writer) {
    if (!writer->conn) {
        writer->conn = PQconnectdb(writer->config->db_url);
    }
    if (PQstatus(writer->conn) != CONNECTION_OK) {
        LOG_ERROR("libpq: %s", PQerrorMessage(writer->conn));
        PQfinish(writer->conn);
        writer->conn = NULL;
        return false;
    }
    return true;
}

static bool insert_pumpfun(struct db_writer *writer, const yurei_event_t *event) {
    if (!ensure_connection(writer))
        return false;
    char mint_b58[64];
    char trader_b58[64];
    char creator_b58[64];
    char slot_buf[32];
    if (base58_encode(event->data.pumpfun_trade.mint, 32, mint_b58, sizeof(mint_b58)) < 0 ||
        base58_encode(event->data.pumpfun_trade.trader, 32, trader_b58, sizeof(trader_b58)) < 0 ||
        base58_encode(event->data.pumpfun_trade.creator, 32, creator_b58, sizeof(creator_b58)) < 0) {
        LOG_ERROR("base58 encode failed for PumpFun event");
        return false;
    }

    char sol_amount[32];
    char token_amount[32];
    char fee_bps[32];
    char fee_amount[32];
    char creator_fee_bps[32];
    char creator_fee_amount[32];
    char virtual_sol[32];
    char virtual_token[32];
    char real_sol[32];
    char real_token[32];
    snprintf(slot_buf, sizeof(slot_buf), "%lu", event->data.pumpfun_trade.slot);
    snprintf(sol_amount, sizeof(sol_amount), "%lu", event->data.pumpfun_trade.sol_amount);
    snprintf(token_amount, sizeof(token_amount), "%lu", event->data.pumpfun_trade.token_amount);
    snprintf(fee_bps, sizeof(fee_bps), "%lu", event->data.pumpfun_trade.fee_basis_points);
    snprintf(fee_amount, sizeof(fee_amount), "%lu", event->data.pumpfun_trade.fee_lamports);
    snprintf(creator_fee_bps, sizeof(creator_fee_bps), "%lu", event->data.pumpfun_trade.creator_fee_basis_points);
    snprintf(creator_fee_amount, sizeof(creator_fee_amount), "%lu", event->data.pumpfun_trade.creator_fee_lamports);
    snprintf(virtual_sol, sizeof(virtual_sol), "%lu", event->data.pumpfun_trade.virtual_sol_reserves);
    snprintf(virtual_token, sizeof(virtual_token), "%lu", event->data.pumpfun_trade.virtual_token_reserves);
    snprintf(real_sol, sizeof(real_sol), "%lu", event->data.pumpfun_trade.real_sol_reserves);
    snprintf(real_token, sizeof(real_token), "%lu", event->data.pumpfun_trade.real_token_reserves);

    const char *params[] = {
        slot_buf,
        event->signature,
        mint_b58,
        trader_b58,
        creator_b58,
        event->data.pumpfun_trade.is_buy ? "BUY" : "SELL",
        sol_amount,
        token_amount,
        fee_bps,
        fee_amount,
        creator_fee_bps,
        creator_fee_amount,
        virtual_sol,
        virtual_token,
        real_sol,
        real_token
    };
    PGresult *res = PQexecParams(
        writer->conn,
        "INSERT INTO pumpfun_trades (slot, tx_signature, mint, trader, creator, side, sol_amount, token_amount, fee_bps, fee_lamports, creator_fee_bps, creator_fee_lamports, virtual_sol_reserves, virtual_token_reserves, real_sol_reserves, real_token_reserves)"
        " VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13,$14,$15,$16)",
        (int)(sizeof(params) / sizeof(params[0])),
        NULL,
        params,
        NULL,
        NULL,
        0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        LOG_ERROR("pumpfun insert failed: %s", PQerrorMessage(writer->conn));
        PQclear(res);
        return false;
    }
    PQclear(res);
    return true;
}

static bool insert_raydium(struct db_writer *writer, const yurei_event_t *event) {
    if (!ensure_connection(writer))
        return false;
    char amm_b58[64];
    char owner_b58[64];
    char slot_buf[32];
    if (base58_encode(event->data.raydium_swap.amm, 32, amm_b58, sizeof(amm_b58)) < 0 ||
        base58_encode(event->data.raydium_swap.user_source_owner, 32, owner_b58, sizeof(owner_b58)) < 0) {
        LOG_ERROR("base58 encode failed for Raydium event");
        return false;
    }

    char amount_in[32];
    char amount_out[32];
    snprintf(slot_buf, sizeof(slot_buf), "%lu", event->data.raydium_swap.slot);
    snprintf(amount_in, sizeof(amount_in), "%lu", event->data.raydium_swap.amount_in);
    snprintf(amount_out, sizeof(amount_out), "%lu", event->data.raydium_swap.amount_out);

    const char *params[] = {
        slot_buf,
        event->signature,
        amm_b58,
        owner_b58,
        amount_in,
        amount_out
    };
    PGresult *res = PQexecParams(
        writer->conn,
        "INSERT INTO raydium_swaps (slot, tx_signature, pool, user_owner, amount_in, amount_out) VALUES ($1,$2,$3,$4,$5,$6)",
        6,
        NULL,
        params,
        NULL,
        NULL,
        0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        LOG_ERROR("raydium insert failed: %s", PQerrorMessage(writer->conn));
        PQclear(res);
        return false;
    }
    PQclear(res);
    return true;
}

static void *db_writer_main(void *arg) {
    struct db_writer *writer = arg;
    while (writer->running) {
        yurei_event_t event;
        if (!event_queue_pop(writer->queue, &event, true)) {
            if (!writer->running)
                break;
            continue;
        }
        switch (event.type) {
        case YUREI_EVENT_PUMPFUN_TRADE:
            insert_pumpfun(writer, &event);
            break;
        case YUREI_EVENT_RAYDIUM_SWAP:
            insert_raydium(writer, &event);
            break;
        default:
            break;
        }
    }
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
    if (pthread_create(&writer->thread, NULL, db_writer_main, writer) != 0) {
        free(writer);
        return NULL;
    }
    return writer;
}

void db_writer_stop(db_writer_t *writer) {
    if (!writer)
        return;
    writer->running = false;
    event_queue_close(writer->queue);
    pthread_join(writer->thread, NULL);
    free(writer);
}
