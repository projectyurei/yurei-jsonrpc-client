#!/usr/bin/env bash
# Apply the default Project Yurei schema to the configured database
set -euo pipefail

CONFIG_FILE=${CONFIG_FILE:-.env}
SCHEMA_FILE=${SCHEMA_FILE:-schema.sql}

if [[ ! -f "${CONFIG_FILE}" ]]; then
    echo "Config file ${CONFIG_FILE} not found" >&2
    exit 1
fi

if [[ ! -f "${SCHEMA_FILE}" ]]; then
    echo "Schema file ${SCHEMA_FILE} not found" >&2
    exit 1
fi

set -a
# shellcheck disable=SC1090
source <(grep -v '^#' "${CONFIG_FILE}" | sed '/^$/d' | sed 's/\r$//')
set +a

: "${YUREI_PG_CONNINFO:?YUREI_PG_CONNINFO must be set in .env}"

if ! command -v psql >/dev/null 2>&1; then
    echo "psql not found; please install PostgreSQL client tools" >&2
    exit 1
fi

psql "${YUREI_PG_CONNINFO}" -f "${SCHEMA_FILE}"
