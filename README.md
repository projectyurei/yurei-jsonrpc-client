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

### v1.1.0 Enhancements

- **Helius RPC Integration** - Default endpoint now uses Helius mainnet RPC for optimal performance
- **Enhanced Logging** - ANSI colors, TRACE/DEBUG/INFO/WARN/ERROR levels, microsecond timestamps
- **Metrics System** - Atomic counters for requests, latency tracking, events processed
- **Rate Limiting** - Token bucket rate limiter to prevent RPC throttling
- **Build Optimizations** - Release mode with `-O3 -flto -march=native`

## Getting Started

### Dependencies

Install the following development packages (Debian/Ubuntu names shown):

```bash
sudo apt install build-essential cmake libwebsockets-dev libcurl4-openssl-dev \
    libcjson-dev libpq-dev
```

### Configure

Copy `.env.example` to `.env` and update the values for your RPC node and database:

```bash
cp .env.example .env
```

#### Helius RPC Setup (Recommended)

1. Get a free API key at [https://dev.helius.xyz/](https://dev.helius.xyz/)
2. Set the following in your `.env`:

```bash
YUREI_RPC_ENDPOINT=https://mainnet.helius-rpc.com
YUREI_WSS_ENDPOINT=wss://mainnet.helius-rpc.com
YUREI_RPC_API_KEY=your-helius-api-key
```

#### Key Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `YUREI_RPC_ENDPOINT` | `https://mainnet.helius-rpc.com` | HTTP RPC endpoint |
| `YUREI_WSS_ENDPOINT` | `wss://mainnet.helius-rpc.com` | WebSocket RPC endpoint |
| `YUREI_RPC_API_KEY` | (empty) | Helius API key |
| `YUREI_RPC_MODE` | `ws` | Connection mode: `ws`, `http`, or `dual` |
| `YUREI_LOG_LEVEL` | `info` | Log level: `trace`, `debug`, `info`, `warn`, `error` |
| `YUREI_LOG_COLOR` | `1` | Enable ANSI colors: `1`/`true` or `0`/`false` |
| `YUREI_RATE_LIMIT` | `10` | Requests per second (0 to disable) |
| `YUREI_BATCH_SIZE` | `20` | JSON-RPC batch size |
| `YUREI_PUMPFUN_PROGRAM` | `6EF8rrecthR5Dkzon8Nwu78hRvfCKubJ14M5uBEwF6P` | PumpFun program ID |
| `YUREI_RAYDIUM_PROGRAM` | `675kPX9MHTjS2zt1qfr1NYHuzeLXfQM9H24wFSUt1Mp8` | Raydium program ID |
| `YUREI_PG_CONNINFO` | (see .env.example) | PostgreSQL connection string |

### Bootstrap the database

Apply the default schema (or point at an existing staging schema) before starting the client:

```bash
scripts/db_init.sh               # uses schema.sql and the connection info in .env
```

Feel free to edit `schema.sql` if you need additional columns or a different layout.

### Build

```bash
# Debug build
cmake -S . -B build
cmake --build build

# Release build (optimized)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Run

```bash
./build/yurei-jsonrpc-client -c .env
```

Additional command-line options:
- `-c, --config <path>` - Specify config file path (default: `.env`)
- `-v, --version` - Print version and exit
- `-h, --help` - Print usage and exit

Set `YUREI_RPC_MODE=http` to rely solely on HTTP polling, or `dual` to keep both
threads active. The process prints structured logs describing reconnection and
backpressure events.

### Metrics

The client logs metrics every 60 seconds:

```
=== METRICS ===
Requests: total=120 success=118 failed=2 (98.3% success)
Latency: avg=45230us min=12000us max=180000us
Events processed: 1542 | Bytes received: 1280.50 KB | WS reconnects: 0
```

### Rate Limit Guidance

Public RPC endpoints aggressively police abusive clients. The default rate limit
is 10 requests per second. Adjust `YUREI_RATE_LIMIT` based on your RPC tier:

| Provider | Recommended RPS |
|----------|-----------------|
| Helius Free | 10 |
| Helius Developer | 25 |
| Helius Business | 50+ |
| PublicNode | 5-10 |

### Verification & Benchmarking

- Run ad-hoc checks with `psql "$YUREI_PG_CONNINFO" -c "SELECT COUNT(*) FROM pumpfun_trades;"`
- For an automated snapshot, execute `scripts/benchmark.sh .env`

## Schema Compatibility

The DB writer intentionally does not interpret PumpFun/Raydium payloads; it stores the
binary blobs for downstream decoders. Use the table-name options in `.env` to point at
either the legacy schema (default) or any custom staging tables you prefer.

## License

Released under the MIT License. See `LICENSE` for details.

