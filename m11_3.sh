#!/usr/bin/env bash
set -euo pipefail

SERVER="${SERVER:-http://localhost:8082}"
WORKER_HOST="${WORKER_HOST:-127.0.0.1}"
WORKER_PORT="${WORKER_PORT:-8083}"
WORKER_ID="${WORKER_ID:-worker-1}"
QUEUE="${QUEUE:-default}"
USER="${USER:-admin}"
PASS="${PASS:-123456}"
TOKEN=""

HAS_JQ=0
command -v jq >/dev/null 2>&1 && HAS_JQ=1
if [[ $HAS_JQ -ne 1 ]]; then
  echo "ERROR: jq is required but not installed." >&2
  exit 1
fi

pretty() {
  if [[ $HAS_JQ -eq 1 ]]; then jq .; else cat; fi
}

title() {
  echo
  echo "=============================="
  echo "▶ $1"
  echo "------------------------------"
}

req() {
  # usage: req METHOD URL JSON_STRING
  local method="$1"; shift
  local url="$1"; shift
  local data="$1"; shift
  if [[ -n "${TOKEN}" ]]; then
    curl -sS -X "$method" "$url" -H "Authorization: Bearer ${TOKEN}" -H "Content-Type: application/json" -d "$data"
  else
    curl -sS -X "$method" "$url" -H "Content-Type: application/json" -d "$data"
  fi
}

get_workers() {
  curl -sS "$SERVER/api/workers"
}

echo "Using server: $SERVER"
echo "Worker: id=$WORKER_ID, host=$WORKER_HOST, port=$WORKER_PORT, queue=$QUEUE"
echo "TTL: 10s (from isAlive())"

echo "=> 登录获取 TOKEN ..."
TOKEN="$(curl -s -X POST "$SERVER/api/login" \
  -H "Content-Type: application/json" \
  -d "{\"username\":\"$USER\",\"password\":\"$PASS\"}" \
  | jq -r '.data.token')"
if [[ -z "${TOKEN}" || "${TOKEN}" == "null" ]]; then
  echo "ERROR: login failed, no token in response"
  exit 1
fi

title "CASE 1: Register worker (expect alive=true, last_seen_ms_ago small)"
req POST "$SERVER/api/workers/register" "{
  \"id\": \"$WORKER_ID\",
  \"host\": \"$WORKER_HOST\",
  \"port\": $WORKER_PORT,
  \"queues\": [\"$QUEUE\"],
  \"labels\": [\"shell\"],
  \"running_tasks\": 0,
  \"max_running_tasks\": 1
}" | pretty

get_workers | pretty

title "CASE 2: Wait 2s, last_seen_ms_ago should grow"
sleep 2
get_workers | pretty

title "CASE 3: Wait 11s (TTL=10s) -> expect alive=false"
# 现在距离上次心跳应该已经 >10s
sleep 11
get_workers | pretty

title "CASE 4: Remote DAG while worker is dead -> expect NO worker available / task fail"
# 使用新 Remote payload 规范：remote.payload_type + remote.payload_json
REMOTE_PAYLOAD_DEAD="$(jq -c -n --arg id "R_dead" --arg cmd "echo SHOULD_NOT_RUN" \
  '{id:$id, exec_type:"Shell", exec_command:$cmd}')"
req POST "$SERVER/api/dag/run" "{
  \"config\": { \"fail_policy\": \"FailFast\", \"max_parallel\": 1 },
  \"tasks\": [
    {
      \"id\": \"R_dead\",
      \"deps\": [],
      \"exec_type\": \"Remote\",
      \"queue\": \"$QUEUE\",
      \"exec_params\": {
        \"remote.payload_type\": \"task\",
        \"remote.payload_json\": $REMOTE_PAYLOAD_DEAD
      },
      \"timeout_ms\": 5000,
      \"capture_output\": true
    }
  ]
}" | pretty

title "CASE 5: Heartbeat worker -> expect alive=true again"
req POST "$SERVER/api/workers/heartbeat" "{
  \"id\": \"$WORKER_ID\",
  \"running_tasks\": 0
}" | pretty
get_workers | pretty

title "CASE 6: Remote DAG after heartbeat -> expect success + stdout contains hello"
REMOTE_PAYLOAD_OK="$(jq -c -n --arg id "R_ok" --arg cmd "echo hello_from_remote_OK" \
  '{id:$id, exec_type:"Shell", exec_command:$cmd}')"
req POST "$SERVER/api/dag/run" "{
  \"config\": { \"fail_policy\": \"FailFast\", \"max_parallel\": 1 },
  \"tasks\": [
    {
      \"id\": \"R_ok\",
      \"deps\": [],
      \"exec_type\": \"Remote\",
      \"queue\": \"$QUEUE\",
      \"exec_params\": {
        \"remote.payload_type\": \"task\",
        \"remote.payload_json\": $REMOTE_PAYLOAD_OK
      },
      \"timeout_ms\": 5000,
      \"capture_output\": true
    }
  ]
}" | pretty

echo
echo "✅ M11.3 flow done."
echo "Tip: If CASE 4 didn't fail, check pickWorkerForQueue() whether it filters !alive based on isAlive()."
