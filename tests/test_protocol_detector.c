// Project Yurei - High-performance Solana data engine
// Copyright 2025 Project Yurei. All rights reserved.
// https://x.com/yureiai

#include <assert.h>
#include <string.h>

#include "protocol_detector.h"

int main(void) {
    uint8_t pumpfun_program[32];
    for (int i = 0; i < 32; ++i)
        pumpfun_program[i] = (uint8_t)(i + 5);

    yurei_protocol_detector_t detector;
    protocol_detector_init(&detector, pumpfun_program, true, NULL, false);

    const uint8_t *accounts[1];
    size_t lens[1];
    accounts[0] = pumpfun_program;
    lens[0] = 32;

    yurei_protocol_t proto = protocol_detector_match_accounts(&detector, accounts, lens, 1);
    assert(proto == YUREI_PROTOCOL_PUMPFUN);

    uint8_t random_account[32] = {0};
    accounts[0] = random_account;
    proto = protocol_detector_match_accounts(&detector, accounts, lens, 1);
    assert(proto == YUREI_PROTOCOL_NONE);
    return 0;
}
