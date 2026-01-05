#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PYTHON_BIN=""
if command -v python3 >/dev/null 2>&1; then
  PYTHON_BIN="python3"
elif command -v python >/dev/null 2>&1; then
  PYTHON_BIN="python"
fi


SERVER_BIN="${TASKHUB_SERVER_BIN:-}"
if [[ -z "${SERVER_BIN}" ]]; then
  for cand in \
    "${ROOT_DIR}/build/bin/taskhub_server" \
    "${ROOT_DIR}/cmake-build-debug/bin/taskhub_server" \
    "${ROOT_DIR}/cmake-build-release/bin/taskhub_server" \
    "${ROOT_DIR}/build/taskhub_server"
  do
    if [[ -x "${cand}" ]]; then
      SERVER_BIN="${cand}"
      break
    fi
  done
fi

if [[ -z "${SERVER_BIN}" ]]; then
  echo "taskhub_server not found. Build it first or set TASKHUB_SERVER_BIN."
  exit 1
fi

MASTER_HOST="127.0.0.1"
MASTER_PORT=8082
WORKER1_PORT=8083
WORKER2_PORT=8084
MASTER_URL="http://${MASTER_HOST}:${MASTER_PORT}"

port_in_use() {
  local port="$1"
  if command -v lsof >/dev/null 2>&1; then
    lsof -n -iTCP:"${port}" -sTCP:LISTEN >/dev/null 2>&1
    return $?
  fi
  if command -v nc >/dev/null 2>&1; then
    nc -z "${MASTER_HOST}" "${port}" >/dev/null 2>&1
    return $?
  fi
  return 1
}

kill_port_listeners() {
  local port="$1"
  if ! command -v lsof >/dev/null 2>&1; then
    return 1
  fi
  local pids
  pids="$(lsof -t -iTCP:"${port}" -sTCP:LISTEN 2>/dev/null || true)"
  if [[ -n "${pids}" ]]; then
    kill ${pids} >/dev/null 2>&1 || true
  fi
}

ensure_port_free() {
  local port="$1"
  if port_in_use "${port}"; then
    if [[ "${TASKHUB_FORCE_KILL_PORTS:-}" == "1" ]]; then
      kill_port_listeners "${port}" || {
        echo "Port ${port} is in use and lsof not available to free it."
        exit 1
      }
      sleep 0.5
      if port_in_use "${port}"; then
        echo "Port ${port} is still in use after cleanup."
        exit 1
      fi
      echo "Port ${port} was in use; cleaned up with TASKHUB_FORCE_KILL_PORTS=1."
    else
      echo "Port ${port} is already in use. Stop the existing process or rerun with TASKHUB_FORCE_KILL_PORTS=1."
      exit 1
    fi
  fi
}

wait_for_port_close() {
  local port="$1"
  local retries="${2:-20}"
  local delay="${3:-0.2}"
  for _ in $(seq 1 "${retries}"); do
    if ! port_in_use "${port}"; then
      return 0
    fi
    sleep "${delay}"
  done
  return 1
}

ensure_port_free "${MASTER_PORT}"
ensure_port_free "${WORKER1_PORT}"
ensure_port_free "${WORKER2_PORT}"

TMP_DIR="$(mktemp -d "${ROOT_DIR}/.tmp_m11_dispatch.XXXX")"
echo $TMP_DIR
MASTER_DIR="${TMP_DIR}/master"
WORKER1_DIR="${TMP_DIR}/worker1"
WORKER2_DIR="${TMP_DIR}/worker2"
echo $MASTER_DIR
mkdir -p "${MASTER_DIR}" "${WORKER1_DIR}" "${WORKER2_DIR}"

cleanup() {
  echo "Keeping logs at: ${TMP_DIR}"
  # for pid in "${WORKER2_PID:-}" "${WORKER1_PID:-}" "${MASTER_PID:-}"; do
  #   if [[ -n "${pid}" ]]; then
  #     kill "${pid}" >/dev/null 2>&1 || true
  #   fi
  # done
  # # echo "KEEP_TMP: ${KEEP_TMP}"
  # if [[ "${KEEP_TMP:-}" != "1" ]]; then
  #   rm -rf "${TMP_DIR}"
  # else
  #   echo "Keeping logs at: ${TMP_DIR}"
  # fi
}
trap cleanup EXIT

LAST_WORKERS_PAYLOAD=""
WORKERS_COUNT=0

MIGRATIONS_SRC=""
for cand in \
  "${ROOT_DIR}/build/bin/migrations" \
  "${ROOT_DIR}/cmake-build-debug/bin/migrations" \
  "${ROOT_DIR}/cmake-build-release/bin/migrations" \
  "${ROOT_DIR}/build/migrations" \
  "${ROOT_DIR}/server/migrations"
do
  if [[ -d "${cand}" ]]; then
    MIGRATIONS_SRC="${cand}"
    break
  fi
done

DB_TEMPLATE=""
for cand in \
  "${ROOT_DIR}/build/bin/taskhub.db" \
  "${ROOT_DIR}/cmake-build-debug/bin/taskhub.db" \
  "${ROOT_DIR}/cmake-build-release/bin/taskhub.db" \
  "${ROOT_DIR}/taskhub.db"
do
  if [[ -f "${cand}" ]]; then
    DB_TEMPLATE="${cand}"
    break
  fi
done

if [[ -n "${MIGRATIONS_SRC}" ]]; then
  for d in "${MASTER_DIR}" "${WORKER1_DIR}" "${WORKER2_DIR}"; do
    mkdir -p "${d}/migrations"
    cp -R "${MIGRATIONS_SRC}/." "${d}/migrations/"
  done
fi

if [[ -n "${DB_TEMPLATE}" ]]; then
  cp "${DB_TEMPLATE}" "${MASTER_DIR}/taskhub_master.db"
  cp "${DB_TEMPLATE}" "${WORKER1_DIR}/taskhub_worker1.db"
  cp "${DB_TEMPLATE}" "${WORKER2_DIR}/taskhub_worker2.db"
fi

workers_count() {
  local payload
  payload="$(curl -s "${MASTER_URL}/api/workers" 2>/dev/null || true)"
  if [[ -n "${payload}" ]]; then
    LAST_WORKERS_PAYLOAD="${payload}"
  fi
  if [[ -z "${payload}" ]]; then
    WORKERS_COUNT=0
    return 0
  fi
  if [[ -n "${PYTHON_BIN}" ]]; then
    local count
    count="$("${PYTHON_BIN}" -c 'import json,sys; data=json.load(sys.stdin); get=lambda x: x.get("workers") if isinstance(x,dict) and isinstance(x.get("workers"),list) else None; workers=get(data) or get(data.get("data") if isinstance(data,dict) else None) or get(data.get("data",{}).get("data") if isinstance(data,dict) and isinstance(data.get("data"),dict) else None) or []; print(len(workers))' 2>/dev/null <<<"${payload}" || true)"
    if [[ "${count}" =~ ^[0-9]+$ ]]; then
      WORKERS_COUNT="${count}"
      return 0
    fi
  fi
  if command -v jq >/dev/null 2>&1; then
    local count
    count="$(printf '%s' "${payload}" | jq '(.workers // .data.workers // .data.data.workers // []) | length' 2>/dev/null || true)"
    if [[ "${count}" =~ ^[0-9]+$ ]]; then
      WORKERS_COUNT="${count}"
      return 0
    fi
  fi
  WORKERS_COUNT="$(printf '%s' "${payload}" | grep -o '"id"[[:space:]]*:[[:space:]]*"[^"]*"' | wc -l | tr -d ' ')"
}

pretty_print_json() {
  local payload="${1:-}"
  if [[ -z "${payload}" ]]; then
    return 0
  fi
  if [[ -n "${PYTHON_BIN}" ]]; then
    "${PYTHON_BIN}" -c 'import json,sys; data=json.load(sys.stdin); print(json.dumps(data, indent=2))' 2>/dev/null <<<"${payload}" || printf '%s\n' "${payload}"
    return 0
  fi
  if command -v jq >/dev/null 2>&1; then
    printf '%s' "${payload}" | jq . 2>/dev/null || printf '%s\n' "${payload}"
    return 0
  fi
  printf '%s\n' "${payload}"
}

extract_token() {
  local payload="${1:-}"
  if [[ -z "${payload}" ]]; then
    echo ""
    return 0
  fi
  if [[ -n "${PYTHON_BIN}" ]]; then
    "${PYTHON_BIN}" -c 'import json,sys; data=json.load(sys.stdin); print(data.get("data", {}).get("token", ""))' 2>/dev/null <<<"${payload}" || echo ""
    return 0
  fi
  if command -v jq >/dev/null 2>&1; then
    printf '%s' "${payload}" | jq -r '.data.token // ""' 2>/dev/null || echo ""
    return 0
  fi
  echo ""
}

cat > "${MASTER_DIR}/config.json" <<EOF
{
  "server": { "host": "${MASTER_HOST}", "port": ${MASTER_PORT} },
  "work": { "is_work": false },
  "database": { "db_path": "${MASTER_DIR}/taskhub_master.db", "migrations_dir": "./migrations" },
  "log": { "path": "${MASTER_DIR}/master.log" }
}
EOF

cat > "${WORKER1_DIR}/config.json" <<EOF
{
  "server": { "host": "${MASTER_HOST}", "port": ${WORKER1_PORT} },
  "work": {
    "is_work": true,
    "master_host": "${MASTER_HOST}",
    "master_port": ${MASTER_PORT},
    "worker_host": "${MASTER_HOST}",
    "worker_port": ${WORKER1_PORT},
    "heartbeat_interval_ms": 5000,
    "worker_id": "worker-1",
    "queues": ["default"],
    "labels": ["shell"]
  },
  "database": { "db_path": "${WORKER1_DIR}/taskhub_worker1.db", "migrations_dir": "./migrations" },
  "log": { "path": "${WORKER1_DIR}/worker1.log" }
}
EOF

cat > "${WORKER2_DIR}/config.json" <<EOF
{
  "server": { "host": "${MASTER_HOST}", "port": ${WORKER2_PORT} },
  "work": {
    "is_work": true,
    "master_host": "${MASTER_HOST}",
    "master_port": ${MASTER_PORT},
    "worker_host": "${MASTER_HOST}",
    "worker_port": ${WORKER2_PORT},
    "heartbeat_interval_ms": 5000,
    "worker_id": "worker-2",
    "queues": ["default"],
    "labels": ["shell"]
  },
  "database": { "db_path": "${WORKER2_DIR}/taskhub_worker2.db", "migrations_dir": "./migrations" },
  "log": { "path": "${WORKER2_DIR}/worker2.log" }
}
EOF

echo "Starting master..."
(cd "${MASTER_DIR}" && TASKHUB_WORKER_SELECT_STRATEGY="least-load" TASKHUB_DISPATCH_MAX_RETRIES=2 \
  exec "${SERVER_BIN}") > "${MASTER_DIR}/stdout.log" 2>&1 &
MASTER_PID=$!

echo "Starting worker-1 (max_running_tasks=2)..."
(cd "${WORKER1_DIR}" && TASKHUB_WORKER_MAX_RUNNING_TASKS=2 exec "${SERVER_BIN}") > "${WORKER1_DIR}/stdout.log" 2>&1 &
WORKER1_PID=$!

echo "Starting worker-2 (max_running_tasks=1)..."
(cd "${WORKER2_DIR}" && TASKHUB_WORKER_MAX_RUNNING_TASKS=1 exec "${SERVER_BIN}") > "${WORKER2_DIR}/stdout.log" 2>&1 &
WORKER2_PID=$!

echo "Waiting for master to be ready..."
master_ready=0
for _ in {1..30}; do
  if curl -s "${MASTER_URL}/api/workers" >/dev/null 2>&1; then
    master_ready=1
    break
  fi
  sleep 0.5
done
if [[ "${master_ready}" -ne 1 ]]; then
  echo "Master not ready after timeout."
  if [[ -f "${MASTER_DIR}/stdout.log" ]]; then
    tail -n 50 "${MASTER_DIR}/stdout.log" || true
  fi
  exit 1
fi

echo "Waiting for workers to register..."
worker_count=0
for _ in {1..30}; do
  workers_count
  worker_count="${WORKERS_COUNT}"
  if [[ "${worker_count}" -ge 2 ]]; then
    break
  fi
  sleep 0.5
done
if [[ "${worker_count}" -lt 2 ]]; then
  echo "Workers not registered after timeout (count=${worker_count})."
  if [[ -n "${LAST_WORKERS_PAYLOAD}" ]]; then
    echo "Last /api/workers response:"
    pretty_print_json "${LAST_WORKERS_PAYLOAD}"
  fi
  if [[ -f "${MASTER_DIR}/stdout.log" ]]; then
    tail -n 50 "${MASTER_DIR}/stdout.log" || true
  fi
  if [[ -f "${WORKER1_DIR}/stdout.log" ]]; then
    tail -n 50 "${WORKER1_DIR}/stdout.log" || true
  fi
  if [[ -f "${WORKER2_DIR}/stdout.log" ]]; then
    tail -n 50 "${WORKER2_DIR}/stdout.log" || true
  fi
  exit 1
fi

echo "Workers registered:"
pretty_print_json "$(curl -s "${MASTER_URL}/api/workers" 2>/dev/null || true)"

TOKEN="$(extract_token "$(curl -s -X POST "${MASTER_URL}/api/login" \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"123456"}' 2>/dev/null || true)")"
if [[ -z "${TOKEN}" ]]; then
  echo "Failed to get auth token."
  exit 1
fi

AUTH_HEADER="Authorization: Bearer ${TOKEN}"

echo "Submit long task (sleep 5) to occupy worker-1..."
curl -s -X POST "${MASTER_URL}/api/dag/run_async" \
  -H "Content-Type: application/json" \
  -H "${AUTH_HEADER}" \
  -d '{"name":"m11-long","tasks":[{"id":"long-1","name":"long-1","exec_type":"Remote","exec_command":"sleep 5","queue":"default"}]}' \
  >/dev/null

sleep 1
echo "Mark worker-1 as full (running_tasks=2)..."
curl -s -X POST "${MASTER_URL}/api/workers/heartbeat" \
  -H "Content-Type: application/json" \
  -d '{"id":"worker-1","running_tasks":2}' >/dev/null

echo "Submit short task, expect dispatch to worker-2..."
short_resp="$(curl -s -X POST "${MASTER_URL}/api/dag/run" \
  -H "Content-Type: application/json" \
  -H "${AUTH_HEADER}" \
  -d '{"name":"m11-short","tasks":[{"id":"short-1","name":"short-1","exec_type":"Remote","exec_command":"echo worker2","queue":"default"}]}' \
  || true)"
if [[ -n "${short_resp}" ]]; then
  echo "Short task response:"
  pretty_print_json "${short_resp}"
fi

echo "Workers snapshot (full/ max_running_tasks visible):"
pretty_print_json "$(curl -s "${MASTER_URL}/api/workers" 2>/dev/null || true)"

echo "Simulate worker-2 unreachable..."
kill "${WORKER2_PID}" >/dev/null 2>&1 || true
if ! wait_for_port_close "${WORKER2_PORT}" 25 0.2; then
  if [[ "${TASKHUB_FORCE_KILL_PORTS:-}" == "1" ]]; then
    echo "Worker-2 still listening; force killing port ${WORKER2_PORT}..."
    kill -9 "${WORKER2_PID}" >/dev/null 2>&1 || true
    kill_port_listeners "${WORKER2_PORT}" || true
    if ! wait_for_port_close "${WORKER2_PORT}" 10 0.2; then
      echo "Worker-2 port ${WORKER2_PORT} is still open after force kill."
      exit 1
    fi
  else
    echo "Worker-2 port ${WORKER2_PORT} is still open after kill. Failover test would be invalid."
    echo "Stop the process on ${WORKER2_PORT} or rerun with TASKHUB_FORCE_KILL_PORTS=1."
    exit 1
  fi
fi
sleep 1

echo "Mark worker-1 running_tasks=1 (not full, but higher load)..."
curl -s -X POST "${MASTER_URL}/api/workers/heartbeat" \
  -H "Content-Type: application/json" \
  -d '{"id":"worker-1","running_tasks":1}' >/dev/null

echo "Submit task to trigger failover retry..."
failover_resp="$(curl -s -X POST "${MASTER_URL}/api/dag/run" \
  -H "Content-Type: application/json" \
  -H "${AUTH_HEADER}" \
  -d '{"name":"m11-failover","tasks":[{"id":"failover-1","name":"failover-1","exec_type":"Remote","exec_command":"echo failover","queue":"default"}]}' \
  || true)"
if [[ -n "${failover_resp}" ]]; then
  echo "Failover task response:"
  pretty_print_json "${failover_resp}"
fi

sleep 2

echo "Recent master dispatch logs:"
if [[ -f "${MASTER_DIR}/master.log" ]]; then
  tail -n 50 "${MASTER_DIR}/master.log" | grep -E "RemoteExecution: POST|dispatch retry" || true
else
  echo "Master log not found at ${MASTER_DIR}/master.log"
fi

echo "Done. If needed, inspect logs under: ${TMP_DIR}"
