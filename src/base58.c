// Project Yurei - High-performance Solana data engine
// Copyright 2025 Project Yurei. All rights reserved.
// https://x.com/yureiai

#include "base58.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

static const char *ALPHABET = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
static int8_t MAP[128];
static bool map_ready = false;

static void base58_init_map(void) {
    if (map_ready)
        return;
    for (int i = 0; i < 128; ++i)
        MAP[i] = -1;
    for (int i = 0; i < 58; ++i)
        MAP[(int)ALPHABET[i]] = (int8_t)i;
    map_ready = true;
}

int base58_decode(const char *input, uint8_t *out, size_t out_len) {
    base58_init_map();

    size_t len = strlen(input);
    size_t zeros = 0;
    while (zeros < len && input[zeros] == '1')
        zeros++;

    size_t tmp_len = len * 733 / 1000 + 1;
    uint8_t *tmp = calloc(tmp_len, 1);
    if (!tmp)
        return -1;

    for (size_t i = zeros; i < len; ++i) {
        int8_t val = (input[i] & 0x80) ? -1 : MAP[(int)input[i]];
        if (val < 0)
            goto fail;
        int carry = val;
        for (ssize_t k = (ssize_t)tmp_len - 1; k >= 0; --k) {
            carry += 58 * tmp[k];
            tmp[k] = carry % 256;
            carry /= 256;
        }
    }

    size_t start = 0;
    while (start < tmp_len && tmp[start] == 0)
        start++;

    size_t total = zeros + (tmp_len - start);
    if (total > out_len)
        goto fail;

    memset(out, 0, zeros);
    memcpy(out + zeros, tmp + start, tmp_len - start);
    free(tmp);
    return (int)total;

fail:
    free(tmp);
    return -1;
}

int base58_encode(const uint8_t *input, size_t len, char *out, size_t out_len) {
    size_t zeros = 0;
    while (zeros < len && input[zeros] == 0)
        zeros++;

    size_t tmp_len = len * 138 / 100 + 1;
    uint8_t *tmp = calloc(tmp_len, 1);
    if (!tmp)
        return -1;

    for (size_t i = zeros; i < len; ++i) {
        int carry = input[i];
        for (ssize_t j = (ssize_t)tmp_len - 1; j >= 0; --j) {
            carry += 256 * tmp[j];
            tmp[j] = carry % 58;
            carry /= 58;
        }
    }

    size_t start = 0;
    while (start < tmp_len && tmp[start] == 0)
        start++;

    size_t total = zeros + (tmp_len - start);
    if (total + 1 > out_len) {
        free(tmp);
        return -1;
    }

    size_t pos = 0;
    for (; pos < zeros; ++pos)
        out[pos] = '1';
    for (size_t i = start; i < tmp_len; ++i)
        out[pos++] = ALPHABET[tmp[i]];
    out[pos] = '\0';
    free(tmp);
    return (int)pos;
}
