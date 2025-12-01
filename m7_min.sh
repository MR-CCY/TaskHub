#!/usr/bin/env bash
set -euo pipefail

API_BASE="http://localhost:8082"
TOKEN="cFLE6XhTJx2g21gIQONxCI0pjSziHsSZ"  # 用你当前环境里的 TOKEN；或者直接写死：TOKEN="vnu0..."

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

wait_task 37