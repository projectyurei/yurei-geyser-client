// Project Yurei - High-performance Solana data engine
// Copyright 2025 Project Yurei. All rights reserved.
// https://x.com/yureiai

#ifndef YUREI_RAYDIUM_PARSER_H
#define YUREI_RAYDIUM_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "yurei_event.h"

#ifdef __cplusplus
extern "C" {
#endif

bool raydium_parse_swap(const uint8_t *data, size_t len, yurei_raydium_swap_t *out);

#ifdef __cplusplus
}
#endif

#endif
