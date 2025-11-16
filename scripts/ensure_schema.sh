#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${YUREI_DB_URL:-}" ]]; then
    echo "YUREI_DB_URL must be set (e.g. export YUREI_DB_URL=...)" >&2
    exit 1
fi

readonly schema_file="schema.sql"

if [[ ! -f "${schema_file}" ]]; then
    echo "Cannot find ${schema_file}; run from repository root." >&2
    exit 1
fi

psql "$YUREI_DB_URL" <<'SQL'
\set ON_ERROR_STOP on

-- Create tables if they are missing (matches schema.sql)
CREATE TABLE IF NOT EXISTS pumpfun_trades (
    observed_at TIMESTAMPTZ DEFAULT now(),
    slot BIGINT NOT NULL,
    tx_signature TEXT NOT NULL,
    mint TEXT NOT NULL,
    trader TEXT NOT NULL,
    creator TEXT NOT NULL,
    side TEXT NOT NULL,
    sol_amount NUMERIC NOT NULL,
    token_amount NUMERIC NOT NULL,
    fee_bps NUMERIC NOT NULL,
    fee_lamports NUMERIC NOT NULL,
    creator_fee_bps NUMERIC NOT NULL,
    creator_fee_lamports NUMERIC NOT NULL,
    virtual_sol_reserves NUMERIC NOT NULL,
    virtual_token_reserves NUMERIC NOT NULL,
    real_sol_reserves NUMERIC NOT NULL,
    real_token_reserves NUMERIC NOT NULL
);

CREATE TABLE IF NOT EXISTS raydium_swaps (
    observed_at TIMESTAMPTZ DEFAULT now(),
    slot BIGINT NOT NULL,
    tx_signature TEXT NOT NULL,
    pool TEXT NOT NULL,
    user_owner TEXT NOT NULL,
    amount_in NUMERIC NOT NULL,
    amount_out NUMERIC NOT NULL
);

-- Ensure legacy deployments are migrated to NUMERIC quantities
ALTER TABLE IF EXISTS pumpfun_trades
    ALTER COLUMN sol_amount TYPE NUMERIC USING sol_amount::numeric,
    ALTER COLUMN token_amount TYPE NUMERIC USING token_amount::numeric,
    ALTER COLUMN fee_bps TYPE NUMERIC USING fee_bps::numeric,
    ALTER COLUMN fee_lamports TYPE NUMERIC USING fee_lamports::numeric,
    ALTER COLUMN creator_fee_bps TYPE NUMERIC USING creator_fee_bps::numeric,
    ALTER COLUMN creator_fee_lamports TYPE NUMERIC USING creator_fee_lamports::numeric,
    ALTER COLUMN virtual_sol_reserves TYPE NUMERIC USING virtual_sol_reserves::numeric,
    ALTER COLUMN virtual_token_reserves TYPE NUMERIC USING virtual_token_reserves::numeric,
    ALTER COLUMN real_sol_reserves TYPE NUMERIC USING real_sol_reserves::numeric,
    ALTER COLUMN real_token_reserves TYPE NUMERIC USING real_token_reserves::numeric;

ALTER TABLE IF EXISTS raydium_swaps
    ALTER COLUMN amount_in TYPE NUMERIC USING amount_in::numeric,
    ALTER COLUMN amount_out TYPE NUMERIC USING amount_out::numeric;
SQL

echo "Schema ensured successfully."
