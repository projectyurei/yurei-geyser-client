// Project Yurei - High-performance Solana data engine
// Copyright 2025 Project Yurei. All rights reserved.
// https://x.com/yureiai

#ifndef YUREI_BASE64_H
#define YUREI_BASE64_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t base64_decoded_size(const char *input, size_t len);
int base64_decode(const char *input, size_t len, uint8_t *out, size_t out_cap, size_t *produced);

#ifdef __cplusplus
}
#endif

#endif
