CREATE TABLE IF NOT EXISTS pumpfun_trades (
    observed_at TIMESTAMPTZ DEFAULT now(),
    slot BIGINT NOT NULL,
    tx_signature TEXT NOT NULL,
    mint TEXT NOT NULL,
    trader TEXT NOT NULL,
    side TEXT NOT NULL,
    sol_amount NUMERIC NOT NULL,
    token_amount NUMERIC NOT NULL,
    price_lamports NUMERIC NOT NULL
);

CREATE TABLE IF NOT EXISTS raydium_swaps (
    observed_at TIMESTAMPTZ DEFAULT now(),
    slot BIGINT NOT NULL,
    tx_signature TEXT NOT NULL,
    pool TEXT NOT NULL,
    base_amount NUMERIC NOT NULL,
    quote_amount NUMERIC NOT NULL,
    direction TEXT NOT NULL
);