#!/usr/bin/env bash
# Helper script to inspect Project Yurei JSON-RPC client tables
# Usage: scripts/db_check.sh [table-name]
set -euo pipefail

CONFIG_FILE=${CONFIG_FILE:-.env}
TABLE=${1:-}
QUERY=${QUERY:-}

if [[ ! -f "${CONFIG_FILE}" ]]; then
    echo "Config file ${CONFIG_FILE} not found" >&2
    exit 1
fi

set -a
# shellcheck disable=SC1090
source <(grep -v '^#' "${CONFIG_FILE}" | sed '/^$/d' | sed 's/\r$//')
set +a

: "${YUREI_PG_CONNINFO:?YUREI_PG_CONNINFO must be defined in .env}"
PUMPFUN_TABLE=${YUREI_PUMPFUN_TABLE:-pumpfun_trades}
RAYDIUM_TABLE=${YUREI_RAYDIUM_TABLE:-raydium_swaps}

if ! command -v psql >/dev/null 2>&1; then
    echo "psql not found; install PostgreSQL client tools" >&2
    exit 1
fi

if [[ -z "${TABLE}" && -z "${QUERY}" ]]; then
    echo "-- PumpFun row count (${PUMPFUN_TABLE}) --"
    psql "${YUREI_PG_CONNINFO}" -c "SELECT COUNT(*) FROM ${PUMPFUN_TABLE};"
    echo "\n-- Raydium row count (${RAYDIUM_TABLE}) --"
    psql "${YUREI_PG_CONNINFO}" -c "SELECT COUNT(*) FROM ${RAYDIUM_TABLE};"
    echo "\n-- Last 5 PumpFun rows --"
    psql "${YUREI_PG_CONNINFO}" -c "SELECT slot, signature, observed_at FROM ${PUMPFUN_TABLE} ORDER BY observed_at DESC LIMIT 5;"
    echo "\n-- Last 5 Raydium rows --"
    psql "${YUREI_PG_CONNINFO}" -c "SELECT slot, signature, observed_at FROM ${RAYDIUM_TABLE} ORDER BY observed_at DESC LIMIT 5;"
else
    if [[ -n "${QUERY}" ]]; then
        psql "${YUREI_PG_CONNINFO}" -c "${QUERY}"
    else
        psql "${YUREI_PG_CONNINFO}" -c "SELECT * FROM ${TABLE} ORDER BY observed_at DESC LIMIT 10;"
    fi
fi
