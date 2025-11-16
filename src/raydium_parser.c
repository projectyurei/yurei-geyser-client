// Project Yurei - High-performance Solana data engine
// Copyright 2025 Project Yurei. All rights reserved.
// https://x.com/yureiai

#include "raydium_parser.h"

#include <stdbool.h>
#include <string.h>

static bool read_u64(const uint8_t *data, size_t len, size_t *offset, uint64_t *value) {
    if (*offset + 8 > len)
        return false;
    uint64_t v = 0;
    memcpy(&v, data + *offset, sizeof(uint64_t));
    *offset += 8;
    *value = v;
    return true;
}

static const uint8_t *read_pubkey(const uint8_t *data, size_t len, size_t *offset) {
    if (*offset + 32 > len)
        return NULL;
    const uint8_t *ptr = data + *offset;
    *offset += 32;
    return ptr;
}

bool raydium_parse_swap(const uint8_t *data, size_t len, yurei_raydium_swap_t *out) {
    if (!data || !out)
        return false;
    size_t offset = 0;
    if (!read_u64(data, len, &offset, &out->amount_in))
        return false;
    uint64_t minimum_amount_out = 0;
    if (!read_u64(data, len, &offset, &minimum_amount_out))
        return false;
    uint64_t max_amount_in = 0;
    if (!read_u64(data, len, &offset, &max_amount_in))
        return false;
    if (!read_u64(data, len, &offset, &out->amount_out))
        return false;
    (void)minimum_amount_out;
    (void)max_amount_in;

    const uint8_t *token_program = read_pubkey(data, len, &offset);
    (void)token_program;
    const uint8_t *amm = read_pubkey(data, len, &offset);
    if (!amm)
        return false;
    memcpy(out->amm, amm, 32);
    const uint8_t *amm_authority = read_pubkey(data, len, &offset);
    (void)amm_authority;
    const uint8_t *amm_open_orders = read_pubkey(data, len, &offset);
    (void)amm_open_orders;

    if (offset >= len)
        return false;
    uint8_t has_target = data[offset++];
    if (has_target) {
        if (!read_pubkey(data, len, &offset))
            return false;
    }

    const uint8_t *pool_coin = read_pubkey(data, len, &offset);
    (void)pool_coin;
    const uint8_t *pool_pc = read_pubkey(data, len, &offset);
    (void)pool_pc;
    const uint8_t *serum_program = read_pubkey(data, len, &offset);
    (void)serum_program;
    const uint8_t *serum_market = read_pubkey(data, len, &offset);
    (void)serum_market;
    const uint8_t *serum_bids = read_pubkey(data, len, &offset);
    (void)serum_bids;
    const uint8_t *serum_asks = read_pubkey(data, len, &offset);
    (void)serum_asks;
    const uint8_t *serum_event_queue = read_pubkey(data, len, &offset);
    (void)serum_event_queue;
    const uint8_t *serum_coin_vault = read_pubkey(data, len, &offset);
    (void)serum_coin_vault;
    const uint8_t *serum_pc_vault = read_pubkey(data, len, &offset);
    (void)serum_pc_vault;
    const uint8_t *serum_signer = read_pubkey(data, len, &offset);
    (void)serum_signer;
    const uint8_t *user_source = read_pubkey(data, len, &offset);
    (void)user_source;
    const uint8_t *user_destination = read_pubkey(data, len, &offset);
    (void)user_destination;
    const uint8_t *user_owner = read_pubkey(data, len, &offset);
    if (!user_owner)
        return false;
    memcpy(out->user_source_owner, user_owner, 32);
    return true;
}
