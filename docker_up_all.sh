#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COMPOSE_FILE="${ROOT_DIR}/docker-compose.yml"

if command -v docker-compose >/dev/null 2>&1; then
  COMPOSE_CMD=(docker-compose)
elif command -v docker >/dev/null 2>&1 && docker compose version >/dev/null 2>&1; then
  COMPOSE_CMD=(docker compose)
else
  echo "docker compose not found (install Docker Desktop or docker-compose)" >&2
  exit 1
fi

if [[ "${TASKHUB_SKIP_BUILD:-0}" != "1" ]]; then
  "${COMPOSE_CMD[@]}" -f "${COMPOSE_FILE}" build
fi

"${COMPOSE_CMD[@]}" -f "${COMPOSE_FILE}" up -d master worker1 worker2

echo "Containers started:"
"${COMPOSE_CMD[@]}" -f "${COMPOSE_FILE}" ps

