// Project Yurei - High-performance Solana data engine
// Copyright 2025 Project Yurei. All rights reserved.
// https://x.com/yureiai

#ifndef YUREI_PUMPFUN_PARSER_H
#define YUREI_PUMPFUN_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "yurei_event.h"

#ifdef __cplusplus
extern "C" {
#endif

bool pumpfun_parse_trade(const uint8_t *data, size_t len, yurei_pumpfun_trade_t *out);
bool pumpfun_parse_log_line(const char *log_line, yurei_pumpfun_trade_t *out);

#ifdef __cplusplus
}
#endif

#endif
