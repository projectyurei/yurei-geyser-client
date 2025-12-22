// Project Yurei - High-performance Solana data engine
// Copyright 2025 Project Yurei. All rights reserved.
// https://x.com/yureiai

#include "yurei_config.h"

#include "base58.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>

static void copy_env(const char *name, char *dest, size_t dest_len, const char *fallback) {
    if (!dest || dest_len == 0)
        return;
    const char *value = getenv(name);
    const char *source = (value && *value) ? value : fallback;
    if (!source) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, source, dest_len - 1);
    dest[dest_len - 1] = '\0';
}

bool yurei_config_load(yurei_config_t *config) {
    if (!config)
        return false;
    memset(config, 0, sizeof(*config));
    // Geyser gRPC Endpoint Configuration
    // Primary: Helius LaserStream (Yellowstone gRPC compatible, requires API key)
    //   - US East: laserstream-mainnet-ewr.helius-rpc.com
    //   - Europe:  laserstream-mainnet-fra.helius-rpc.com
    //   - Asia:    laserstream-mainnet-tyo.helius-rpc.com
    // Fallback: PublicNode (free, public): solana-yellowstone-grpc.publicnode.com:443
    copy_env("YUREI_GEYSER_ENDPOINT", config->endpoint, sizeof(config->endpoint), "laserstream-mainnet-ewr.helius-rpc.com:443");
    copy_env("YUREI_GEYSER_AUTHORITY", config->authority, sizeof(config->authority), "laserstream-mainnet-ewr.helius-rpc.com");
    copy_env("YUREI_DB_URL", config->db_url, sizeof(config->db_url), "postgres://postgres:postgres@127.0.0.1:5432/yurei");
    copy_env("YUREI_GEYSER_AUTH_TOKEN", config->auth_token, sizeof(config->auth_token), "");

    const char *pumpfun = getenv("YUREI_PUMPFUN_PROGRAM");
    if (pumpfun && *pumpfun) {
        if (base58_decode(pumpfun, config->pumpfun_program, sizeof(config->pumpfun_program)) != 32) {
            LOG_ERROR("invalid PumpFun program id");
            return false;
        }
        config->pumpfun_enabled = true;
    }
    const char *raydium = getenv("YUREI_RAYDIUM_PROGRAM");
    if (raydium && *raydium) {
        if (base58_decode(raydium, config->raydium_program, sizeof(config->raydium_program)) != 32) {
            LOG_ERROR("invalid Raydium program id");
            return false;
        }
        config->raydium_enabled = true;
    }

    const char *slot = getenv("YUREI_RESUME_FROM_SLOT");
    if (slot && *slot) {
        config->from_slot = strtoull(slot, NULL, 10);
        config->from_slot_set = true;
    }

    const char *queue_cap = getenv("YUREI_QUEUE_CAPACITY");
    config->queue_capacity = queue_cap && *queue_cap ? strtoul(queue_cap, NULL, 10) : 65536;
    if (config->queue_capacity < 1024)
        config->queue_capacity = 1024;
    return true;
}
