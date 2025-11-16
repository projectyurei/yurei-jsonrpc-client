# Project Yurei JSON-RPC Client

> Project Yurei — the on-chain ghost decoding Solana in real time

This repository contains the MIT-licensed fallback ingestion pipeline for Project Yurei,
the on-chain AI agent from [@yureiai](https://x.com/yureiai). It consumes Solana
JSON-RPC/WebSocket feeds using only public infrastructure, decodes PumpFun and Raydium
logs, and streams their binary payloads into a Postgres-backed analytics stack.

## Features

- libwebsockets client subscribing to `logsSubscribe` filtered by Program IDs
- Optional libcurl poller for `getLogs` backfill or air-gapped mode
- cJSON text parsing with Base64 decoding of `Program data:` payloads
- Thread-safe ring buffer between network and database workers
- libpq batch writer with configurable table names (defaults mirror legacy schema)
- Fully configurable via `.env` or environment variables

## Getting Started

### Dependencies

Install the following development packages (Debian/Ubuntu names shown):

```
sudo apt install build-essential cmake libwebsockets-dev libcurl4-openssl-dev \
    libcjson-dev libpq-dev
```

### Configure

Copy `.env.example` to `.env` and update the values for your RPC node and database:

```
cp .env.example .env
```

Key variables:

- `YUREI_WSS_ENDPOINT` / `YUREI_RPC_ENDPOINT` – public Solana provider URLs
- `YUREI_PUMPFUN_PROGRAM`, `YUREI_RAYDIUM_PROGRAM` – program IDs to monitor
- `YUREI_RPC_MODE` – `ws` (default), `http`, or `dual`
- `YUREI_PUMPFUN_TABLE`, `YUREI_RAYDIUM_TABLE` – destination tables for raw logs
- `YUREI_PG_CONNINFO` – libpq connection string to your database

### Bootstrap the database

Apply the default schema (or point at an existing staging schema) before starting the client:

```
scripts/db_init.sh               # uses schema.sql and the connection info in .env
```

Feel free to edit `schema.sql` if you need additional columns or a different layout.

### Build

```
cmake -S . -B build
cmake --build build
```

### Run

```
./build/yurei-jsonrpc-client -c .env
```

Set `YUREI_RPC_MODE=http` to rely solely on HTTP polling, or `dual` to keep both
threads active. The process prints structured logs describing reconnection and
backpressure events.

### Rate Limit Guidance

Public RPC endpoints aggressively police abusive clients. Keep `YUREI_POLL_INTERVAL_MS`
>= 1000 (1s) when polling, and allow the default exponential backoff for WebSockets.
If you need guaranteed throughput, upgrade to a paid tier at your preferred provider.

### Verification & Benchmarking

- Run ad-hoc checks with `psql "$YUREI_PG_CONNINFO" -c "SELECT COUNT(*) FROM pumpfun_trades;"` (or
  whatever table names you configured). If rows keep increasing while the client runs, ingestion is
  healthy.
- For an automated snapshot, execute `scripts/benchmark.sh .env`. The helper runs the binary for a
  configurable duration (`DURATION_SECONDS` env), compares table counts before/after, prints an
  approximate TPS per protocol, and stores the raw log in `./yurei-benchmark.log`.

## Schema Compatibility

The DB writer intentionally does not interpret PumpFun/Raydium payloads; it stores the
binary blobs for downstream decoders. Use the table-name options in `.env` to point at
either the legacy schema (default) or any custom staging tables you prefer.

## License

Released under the MIT License. See `LICENSE` for details.
