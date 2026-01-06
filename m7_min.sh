#!/usr/bin/env bash
set -euo pipefail

API_BASE="${API_BASE:-http://localhost:8082}"
USER="${USER:-admin}"
PASS="${PASS:-123456}"
TASK_ID="${1:-}"

if [[ -z "${TASK_ID}" ]]; then
  echo "Usage: $0 <task_id>"
  exit 1
fi

if ! command -v jq >/dev/null 2>&1; then
  echo "ERROR: jq is required but not installed." >&2
  exit 1
fi

login() {
  local resp
  resp=$(curl -s -X POST "$API_BASE/api/login" \
    -H "Content-Type: application/json" \
    -d "{\"username\":\"$USER\",\"password\":\"$PASS\"}")

  TOKEN=$(echo "$resp" | jq -r '.data.token')
  if [[ -z "${TOKEN:-}" || "$TOKEN" == "null" ]]; then
    echo "ERROR: login failed, no token in response" >&2
    exit 1
  fi
}

wait_task() {
  local id="$1"
  echo
  echo "=> 轮询任务 $id 状态1..."

  while true; do
    local json
    json=$(curl -s "$API_BASE/api/tasks/$id" \
      -H "Authorization: Bearer $TOKEN")

    echo "[RAW] $json"

    local status
    status=$(echo "$json" | jq -r '.data.status')
    local exit_code
    exit_code=$(echo "$json" | jq -r '.data.exit_code')

    echo "[$(date +%T)] task $id status=$status exit=$exit_code"

    if [[ "$status" == "success" || "$status" == "failed" || "$status" == "timeout" || "$status" == "canceled" ]]; then
      echo "最终任务信息:"
      echo "$json" | jq
      break
    fi

    sleep 1
  done
}

login
wait_task "${TASK_ID}"
