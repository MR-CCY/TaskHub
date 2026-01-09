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

# This mode is for:
# - master runs on host machine (not in Docker)
# - workers run in Docker, and master reaches workers via host port mappings (8083/8084)
#
# Defaults (override as needed):
#   TASKHUB_WORK_MASTER_HOST=host.docker.internal
#   TASKHUB_WORK_MASTER_PORT=8082
#   TASKHUB_WORKER1_ADVERTISE_HOST=127.0.0.1
#   TASKHUB_WORKER2_ADVERTISE_HOST=127.0.0.1

export TASKHUB_WORK_MASTER_HOST="${TASKHUB_WORK_MASTER_HOST:-host.docker.internal}"
export TASKHUB_WORK_MASTER_PORT="${TASKHUB_WORK_MASTER_PORT:-8082}"
export TASKHUB_WORKER1_ADVERTISE_HOST="${TASKHUB_WORKER1_ADVERTISE_HOST:-127.0.0.1}"
export TASKHUB_WORKER2_ADVERTISE_HOST="${TASKHUB_WORKER2_ADVERTISE_HOST:-127.0.0.1}"

if [[ "${TASKHUB_SKIP_BUILD:-0}" != "1" ]]; then
  "${COMPOSE_CMD[@]}" -f "${COMPOSE_FILE}" build worker1 worker2
fi

echo "Stopping docker master (if running)..."
"${COMPOSE_CMD[@]}" -f "${COMPOSE_FILE}" stop master || true

echo "Starting workers (force recreate to apply env overrides)..."
"${COMPOSE_CMD[@]}" -f "${COMPOSE_FILE}" up -d --force-recreate worker1 worker2

echo "Containers running:"
"${COMPOSE_CMD[@]}" -f "${COMPOSE_FILE}" ps

echo
echo "Note: ensure your host master is running at ${TASKHUB_WORK_MASTER_HOST}:${TASKHUB_WORK_MASTER_PORT} (listening on 0.0.0.0)."

