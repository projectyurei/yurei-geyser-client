// Project Yurei - High-performance Solana data engine
// Copyright 2025 Project Yurei. All rights reserved.
// https://x.com/yureiai

#ifndef YUREI_PROTOCOL_DETECTOR_H
#define YUREI_PROTOCOL_DETECTOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "yurei_event.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    YUREI_PROTOCOL_NONE = 0,
    YUREI_PROTOCOL_PUMPFUN,
    YUREI_PROTOCOL_RAYDIUM
} yurei_protocol_t;

typedef struct {
    uint8_t program_id[32];
    bool enabled;
} yurei_protocol_pattern_t;

typedef struct {
    yurei_protocol_pattern_t pumpfun;
    yurei_protocol_pattern_t raydium;
} yurei_protocol_detector_t;

void protocol_detector_init(yurei_protocol_detector_t *detector,
                            const uint8_t pumpfun_program[32], bool pumpfun_enabled,
                            const uint8_t raydium_program[32], bool raydium_enabled);

yurei_protocol_t protocol_detector_match_accounts(const yurei_protocol_detector_t *detector,
                                                  const uint8_t *const *accounts,
                                                  const size_t *account_lens,
                                                  size_t n_accounts);

bool protocol_detector_match_program(const yurei_protocol_pattern_t *pattern,
                                     const uint8_t *data,
                                     size_t len);

#ifdef __cplusplus
}
#endif

#endif
