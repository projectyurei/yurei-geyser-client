// Project Yurei - High-performance Solana data engine
// Copyright 2025 Project Yurei. All rights reserved.
// https://x.com/yureiai

#ifndef YUREI_EVENT_H
#define YUREI_EVENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define YUREI_MAX_SIGNATURE_LEN 88
#define YUREI_MAX_PUBKEY_TEXT 64

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    YUREI_EVENT_NONE = 0,
    YUREI_EVENT_PUMPFUN_TRADE,
    YUREI_EVENT_RAYDIUM_SWAP
} yurei_event_type_t;

typedef struct {
    uint8_t mint[32];
    uint8_t trader[32];
    uint8_t creator[32];
    uint64_t sol_amount;
    uint64_t token_amount;
    bool is_buy;
    uint64_t virtual_sol_reserves;
    uint64_t virtual_token_reserves;
    uint64_t real_sol_reserves;
    uint64_t real_token_reserves;
    uint64_t fee_basis_points;
    uint64_t fee_lamports;
    uint64_t creator_fee_basis_points;
    uint64_t creator_fee_lamports;
    uint64_t slot;
    int64_t timestamp;
} yurei_pumpfun_trade_t;

typedef struct {
    uint8_t amm[32];
    uint8_t user_source_owner[32];
    uint64_t amount_in;
    uint64_t amount_out;
    uint64_t slot;
} yurei_raydium_swap_t;

typedef struct {
    yurei_event_type_t type;
    char signature[YUREI_MAX_SIGNATURE_LEN];
    union {
        yurei_pumpfun_trade_t pumpfun_trade;
        yurei_raydium_swap_t raydium_swap;
    } data;
} yurei_event_t;

#ifdef __cplusplus
}
#endif

#endif
