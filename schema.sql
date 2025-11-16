-- Project Yurei JSON-RPC client default schema
CREATE TABLE IF NOT EXISTS pumpfun_trades (
    observed_at TIMESTAMPTZ DEFAULT now(),
    slot BIGINT NOT NULL,
    signature TEXT PRIMARY KEY,
    program_id TEXT,
    raw_log BYTEA NOT NULL
);

CREATE TABLE IF NOT EXISTS raydium_swaps (
    observed_at TIMESTAMPTZ DEFAULT now(),
    slot BIGINT NOT NULL,
    signature TEXT PRIMARY KEY,
    program_id TEXT,
    raw_log BYTEA NOT NULL
);
