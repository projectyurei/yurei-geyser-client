// Project Yurei - High-performance Solana data engine
// Copyright 2025 Project Yurei. All rights reserved.
// https://x.com/yureiai

#ifndef YUREI_CONFIG_H
#define YUREI_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define YUREI_ENDPOINT_MAX 256
#define YUREI_AUTHORITY_MAX 128
#define YUREI_DB_URL_MAX 512
#define YUREI_AUTH_TOKEN_MAX 512

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char endpoint[YUREI_ENDPOINT_MAX];
    char authority[YUREI_AUTHORITY_MAX];
    char db_url[YUREI_DB_URL_MAX];
    char auth_token[YUREI_AUTH_TOKEN_MAX];
    uint8_t pumpfun_program[32];
    uint8_t raydium_program[32];
    bool pumpfun_enabled;
    bool raydium_enabled;
    uint64_t from_slot;
    bool from_slot_set;
    size_t queue_capacity;
} yurei_config_t;

bool yurei_config_load(yurei_config_t *config);

#ifdef __cplusplus
}
#endif

#endif
