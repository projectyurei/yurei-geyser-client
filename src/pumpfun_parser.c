// Project Yurei - High-performance Solana data engine
// Copyright 2025 Project Yurei. All rights reserved.
// https://x.com/yureiai

#include "pumpfun_parser.h"

#include "base64.h"

#include <string.h>

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

_Static_assert(sizeof(pumpfun_trade_layout_t) == 250, "PumpFun trade layout mismatch");

bool pumpfun_parse_trade(const uint8_t *data, size_t len, yurei_pumpfun_trade_t *out) {
    if (!data || !out || len < sizeof(pumpfun_trade_layout_t))
        return false;
    const pumpfun_trade_layout_t *layout = (const pumpfun_trade_layout_t *)data;
    memcpy(out->mint, layout->mint, sizeof(out->mint));
    memcpy(out->trader, layout->user, sizeof(out->trader));
    memcpy(out->creator, layout->creator, sizeof(out->creator));
    out->sol_amount = layout->sol_amount;
    out->token_amount = layout->token_amount;
    out->is_buy = layout->is_buy != 0;
    out->virtual_sol_reserves = layout->virtual_sol_reserves;
    out->virtual_token_reserves = layout->virtual_token_reserves;
    out->real_sol_reserves = layout->real_sol_reserves;
    out->real_token_reserves = layout->real_token_reserves;
    out->fee_basis_points = layout->fee_basis_points;
    out->fee_lamports = layout->fee;
    out->creator_fee_basis_points = layout->creator_fee_basis_points;
    out->creator_fee_lamports = layout->creator_fee;
    out->timestamp = layout->timestamp;
    out->slot = 0;
    return true;
}

bool pumpfun_parse_log_line(const char *log_line, yurei_pumpfun_trade_t *out) {
    if (!log_line || !out)
        return false;
    const char *needle = "Program data: ";
    const char *pos = strstr(log_line, needle);
    if (!pos)
        return false;
    pos += strlen(needle);
    size_t len = strlen(pos);
    if (len == 0)
        return false;
    uint8_t buffer[512];
    size_t produced = 0;
    if (base64_decode(pos, len, buffer, sizeof(buffer), &produced) != 0)
        return false;
    return pumpfun_parse_trade(buffer, produced, out);
}
