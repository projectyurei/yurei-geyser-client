// Project Yurei - High-performance Solana data engine
// Copyright 2025 Project Yurei. All rights reserved.
// https://x.com/yureiai

#include <assert.h>
#include <string.h>

#include "pumpfun_parser.h"

#pragma pack(push, 1)
typedef struct {
    uint8_t mint[32];
    uint64_t sol_amount;
    uint64_t token_amount;
    uint8_t is_buy;
    uint8_t user[32];
    int64_t timestamp;
    uint64_t virtual_sol_reserves;
    uint64_t virtual_token_reserves;
    uint64_t real_sol_reserves;
    uint64_t real_token_reserves;
    uint8_t fee_recipient[32];
    uint64_t fee_basis_points;
    uint64_t fee;
    uint8_t creator[32];
    uint64_t creator_fee_basis_points;
    uint64_t creator_fee;
    uint8_t track_volume;
    uint64_t total_unclaimed_tokens;
    uint64_t total_claimed_tokens;
    uint64_t current_sol_volume;
    int64_t last_update_timestamp;
} pumpfun_trade_layout_t;
#pragma pack(pop)

int main(void) {
    pumpfun_trade_layout_t layout = {0};
    for (int i = 0; i < 32; ++i) {
        layout.mint[i] = (uint8_t)i;
        layout.user[i] = (uint8_t)(i + 1);
        layout.creator[i] = (uint8_t)(i + 2);
    }
    layout.sol_amount = 42;
    layout.token_amount = 1337;
    layout.is_buy = 1;
    layout.timestamp = 123456789;
    layout.virtual_sol_reserves = 10;
    layout.virtual_token_reserves = 20;
    layout.real_sol_reserves = 30;
    layout.real_token_reserves = 40;
    layout.fee_basis_points = 50;
    layout.fee = 60;
    layout.creator_fee_basis_points = 70;
    layout.creator_fee = 80;

    yurei_pumpfun_trade_t parsed;
    assert(pumpfun_parse_trade((const uint8_t *)&layout, sizeof(layout), &parsed));
    assert(parsed.sol_amount == 42);
    assert(parsed.token_amount == 1337);
    assert(parsed.is_buy);
    assert(parsed.timestamp == 123456789);
    assert(parsed.fee_basis_points == 50);
    assert(parsed.creator_fee_lamports == 80);
    assert(memcmp(parsed.mint, layout.mint, 32) == 0);
    assert(memcmp(parsed.trader, layout.user, 32) == 0);
    assert(memcmp(parsed.creator, layout.creator, 32) == 0);
    return 0;
}
