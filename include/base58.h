// Project Yurei - High-performance Solana data engine
// Copyright 2025 Project Yurei. All rights reserved.
// https://x.com/yureiai

#ifndef YUREI_BASE58_H
#define YUREI_BASE58_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int base58_decode(const char *input, uint8_t *out, size_t out_len);
int base58_encode(const uint8_t *input, size_t len, char *out, size_t out_len);

#ifdef __cplusplus
}
#endif

#endif
