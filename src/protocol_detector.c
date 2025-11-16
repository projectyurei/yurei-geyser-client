// Project Yurei - High-performance Solana data engine
// Copyright 2025 Project Yurei. All rights reserved.
// https://x.com/yureiai

#include "protocol_detector.h"

#include <immintrin.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static const size_t PROGRAM_ID_LEN = 32;

static const uint8_t *fast_memmem(const uint8_t *haystack, size_t haystack_len, const uint8_t *needle, size_t needle_len) {
    if (needle_len == 0 || haystack_len < needle_len)
        return NULL;
#if defined(__AVX2__)
    __m256i needle_vec = _mm256_loadu_si256((const __m256i *)needle);
    for (size_t i = 0; i + needle_len <= haystack_len; i += 32) {
        __m256i chunk = _mm256_loadu_si256((const __m256i *)(haystack + i));
        __m256i cmp = _mm256_cmpeq_epi8(chunk, needle_vec);
        int mask = _mm256_movemask_epi8(cmp);
        if (mask == -1) {
            if (memcmp(haystack + i, needle, needle_len) == 0)
                return haystack + i;
        } else if (mask) {
            while (mask) {
                int bit = __builtin_ctz(mask);
                mask &= mask - 1;
                size_t pos = i + (size_t)bit;
                if (pos + needle_len <= haystack_len && memcmp(haystack + pos, needle, needle_len) == 0)
                    return haystack + pos;
            }
        }
    }
#elif defined(__SSE2__)
    __m128i first = _mm_loadu_si128((const __m128i *)needle);
    for (size_t i = 0; i + needle_len <= haystack_len; i += 16) {
        __m128i chunk = _mm_loadu_si128((const __m128i *)(haystack + i));
        __m128i cmp = _mm_cmpeq_epi8(chunk, first);
        int mask = _mm_movemask_epi8(cmp);
        while (mask) {
            int bit = __builtin_ctz(mask);
            mask &= mask - 1;
            size_t pos = i + (size_t)bit;
            if (pos + needle_len <= haystack_len && memcmp(haystack + pos, needle, needle_len) == 0)
                return haystack + pos;
        }
    }
#else
    for (size_t i = 0; i + needle_len <= haystack_len; ++i) {
        if (memcmp(haystack + i, needle, needle_len) == 0)
            return haystack + i;
    }
#endif
    return NULL;
}

void protocol_detector_init(yurei_protocol_detector_t *detector,
                            const uint8_t pumpfun_program[32], bool pumpfun_enabled,
                            const uint8_t raydium_program[32], bool raydium_enabled) {
    memset(detector, 0, sizeof(*detector));
    if (pumpfun_enabled) {
        memcpy(detector->pumpfun.program_id, pumpfun_program, PROGRAM_ID_LEN);
        detector->pumpfun.enabled = true;
    }
    if (raydium_enabled) {
        memcpy(detector->raydium.program_id, raydium_program, PROGRAM_ID_LEN);
        detector->raydium.enabled = true;
    }
}

bool protocol_detector_match_program(const yurei_protocol_pattern_t *pattern,
                                     const uint8_t *data,
                                     size_t len) {
    if (!pattern->enabled || !data)
        return false;
    return fast_memmem(data, len, pattern->program_id, PROGRAM_ID_LEN) != NULL;
}

yurei_protocol_t protocol_detector_match_accounts(const yurei_protocol_detector_t *detector,
                                                  const uint8_t *const *accounts,
                                                  const size_t *account_lens,
                                                  size_t n_accounts) {
    for (size_t i = 0; i < n_accounts; ++i) {
        if (!accounts[i] || account_lens[i] == 0)
            continue;
        if (detector->pumpfun.enabled && account_lens[i] == PROGRAM_ID_LEN &&
            memcmp(accounts[i], detector->pumpfun.program_id, PROGRAM_ID_LEN) == 0) {
            return YUREI_PROTOCOL_PUMPFUN;
        }
        if (detector->raydium.enabled && account_lens[i] == PROGRAM_ID_LEN &&
            memcmp(accounts[i], detector->raydium.program_id, PROGRAM_ID_LEN) == 0) {
            return YUREI_PROTOCOL_RAYDIUM;
        }
    }
    return YUREI_PROTOCOL_NONE;
}
