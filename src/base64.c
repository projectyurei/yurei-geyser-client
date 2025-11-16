// Project Yurei - High-performance Solana data engine
// Copyright 2025 Project Yurei. All rights reserved.
// https://x.com/yureiai

#include "base64.h"

#include <stdbool.h>
#include <stdint.h>

static uint8_t decode_table[256];
static bool table_ready = false;

static void base64_init_table(void) {
    if (table_ready)
        return;
    for (int i = 0; i < 256; ++i)
        decode_table[i] = 0x80;
    const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int i = 0; i < 64; ++i)
        decode_table[(unsigned char)alphabet[i]] = (uint8_t)i;
    decode_table[(unsigned char)'='] = 0;
    table_ready = true;
}

size_t base64_decoded_size(const char *input, size_t len) {
    size_t padding = 0;
    if (len >= 1 && input[len - 1] == '=')
        padding++;
    if (len >= 2 && input[len - 2] == '=')
        padding++;
    return ((len + 3) / 4) * 3 - padding;
}

int base64_decode(const char *input, size_t len, uint8_t *out, size_t out_cap, size_t *produced) {
    base64_init_table();

    size_t out_len = base64_decoded_size(input, len);
    if (out_cap < out_len)
        return -1;
    size_t out_index = 0;
    uint32_t buffer = 0;
    int bits_collected = 0;
    for (size_t i = 0; i < len; ++i) {
        uint8_t val = decode_table[(unsigned char)input[i]];
        if (val == 0x80)
            continue;
        buffer = (buffer << 6u) | val;
        bits_collected += 6;
        if (bits_collected >= 8) {
            bits_collected -= 8;
            out[out_index++] = (uint8_t)(buffer >> bits_collected);
        }
    }
    if (produced)
        *produced = out_index;
    return 0;
}
