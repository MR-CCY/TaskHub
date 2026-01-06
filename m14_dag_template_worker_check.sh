#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Find Python (required for JSON/db parsing).
PYTHON_BIN=""
if command -v python3 >/dev/null 2>&1; then
  PYTHON_BIN="python3"
elif command -v python >/dev/null 2>&1; then
  PYTHON_BIN="python"
fi
if [[ -z "${PYTHON_BIN}" ]]; then
  echo "python3 or python is required for this script."
  exit 1
fi

SERVER_BIN="${TASKHUB_SERVER_BIN:-}"
if [[ -z "${SERVER_BIN}" ]]; then
  latest_bin=""
  latest_ts=0
  for cand in \
    "${ROOT_DIR}/build/bin/taskhub_server" \
    "${ROOT_DIR}/cmake-build-debug/bin/taskhub_server" \
    "${ROOT_DIR}/cmake-build-release/bin/taskhub_server" \
    "${ROOT_DIR}/build/taskhub_server"
  do
    if [[ -x "${cand}" ]]; then
      ts="$("${PYTHON_BIN}" -c 'import os,sys; print(int(os.path.getmtime(sys.argv[1])))' "${cand}")"
      if [[ "${ts}" -gt "${latest_ts}" ]]; then
        latest_ts="${ts}"
        latest_bin="${cand}"
      fi
    fi
  done
  SERVER_BIN="${latest_bin}"
fi

if [[ -z "${SERVER_BIN}" ]]; then
  echo "taskhub_server not found. Build it first or set TASKHUB_SERVER_BIN."
  exit 1
fi
echo "Using server binary: ${SERVER_BIN}"

MASTER_HOST="127.0.0.1"
MASTER_PORT=8082
WORKER_PORT=8083
MASTER_URL="http://${MASTER_HOST}:${MASTER_PORT}"
WORKER_URL="http://${MASTER_HOST}:${WORKER_PORT}"

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

ensure_port_free "${MASTER_PORT}"
ensure_port_free "${WORKER_PORT}"

TMP_DIR="$(mktemp -d "${ROOT_DIR}/.tmp_m14_dag_template.XXXX")"
MASTER_DIR="${TMP_DIR}/master"
WORKER_DIR="${TMP_DIR}/worker1"
mkdir -p "${MASTER_DIR}" "${WORKER_DIR}"

cleanup() {
  echo "Keeping logs at: ${TMP_DIR}"
}
trap cleanup EXIT

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
  for d in "${MASTER_DIR}" "${WORKER_DIR}"; do
    mkdir -p "${d}/migrations"
    cp -R "${MIGRATIONS_SRC}/." "${d}/migrations/"
  done
else
  echo "Warning: migrations directory not found."
fi

if [[ -n "${DB_TEMPLATE}" ]]; then
  cp "${DB_TEMPLATE}" "${MASTER_DIR}/taskhub_master.db"
  cp "${DB_TEMPLATE}" "${WORKER_DIR}/taskhub_worker1.db"
else
  echo "Warning: DB template not found; a new DB will be created."
fi

cat > "${MASTER_DIR}/config.json" <<EOF
{
  "server": { "host": "${MASTER_HOST}", "port": ${MASTER_PORT} },
  "work": { "is_work": false },
  "worker": { "select_strategy": "least-load" },
  "dispatch": { "max_retries": 2 },
  "database": { "db_path": "${MASTER_DIR}/taskhub_master.db", "migrations_dir": "./migrations" },
  "log": { "path": "${MASTER_DIR}/master.log" }
}
EOF

cat > "${WORKER_DIR}/config.json" <<EOF
{
  "server": { "host": "${MASTER_HOST}", "port": ${WORKER_PORT} },
  "work": {
    "is_work": true,
    "master_host": "${MASTER_HOST}",
    "master_port": ${MASTER_PORT},
    "worker_host": "${MASTER_HOST}",
    "worker_port": ${WORKER_PORT},
    "heartbeat_interval_ms": 2000,
    "max_running_tasks": 1,
    "worker_id": "worker-1",
    "queues": ["default"],
    "labels": ["shell"]
  },
  "database": { "db_path": "${WORKER_DIR}/taskhub_worker1.db", "migrations_dir": "./migrations" },
  "log": { "path": "${WORKER_DIR}/worker1.log" }
}
EOF

echo "Starting master..."
(cd "${MASTER_DIR}" && exec "${SERVER_BIN}") > "${MASTER_DIR}/stdout.log" 2>&1 &
MASTER_PID=$!

echo "Starting worker..."
(cd "${WORKER_DIR}" && exec "${SERVER_BIN}") > "${WORKER_DIR}/stdout.log" 2>&1 &
WORKER_PID=$!

echo "Waiting for master to be ready..."
master_ready=0
for _ in {1..30}; do
  if curl -s "${MASTER_URL}/api/workers" >/dev/null 2>&1; then
    master_ready=1
    break
  fi
  sleep 0.5
done
if [[ "${master_ready}" != "1" ]]; then
  echo "Master not ready."
  exit 1
fi

workers_count() {
  local payload
  payload="$(curl -s "${MASTER_URL}/api/workers" 2>/dev/null || true)"
  if [[ -z "${payload}" ]]; then
    echo "0"
    return 0
  fi
  "${PYTHON_BIN}" -c 'import json,sys; data=json.load(sys.stdin); workers=data.get("data", {}).get("workers") or data.get("workers") or []; print(len(workers))' <<<"${payload}" || echo "0"
}

echo "Waiting for worker registration..."
worker_ready=0
for _ in {1..30}; do
  if [[ "$(workers_count)" -ge 1 ]]; then
    worker_ready=1
    break
  fi
  sleep 0.5
done
if [[ "${worker_ready}" != "1" ]]; then
  echo "Worker not registered."
  tail -n 50 "${MASTER_DIR}/stdout.log" || true
  tail -n 50 "${WORKER_DIR}/stdout.log" || true
  exit 1
fi

extract_token() {
  local payload="${1:-}"
  if [[ -z "${payload}" ]]; then
    echo ""
    return 0
  fi
  "${PYTHON_BIN}" -c 'import json,sys; data=json.load(sys.stdin); print(data.get("data", {}).get("token", ""))' <<<"${payload}" || echo ""
}

MASTER_TOKEN="$(extract_token "$(curl -s -X POST "${MASTER_URL}/api/login" \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"123456"}' 2>/dev/null || true)")"
if [[ -z "${MASTER_TOKEN}" ]]; then
  echo "Failed to get master auth token."
  exit 1
fi

WORKER_TOKEN="$(extract_token "$(curl -s -X POST "${WORKER_URL}/api/login" \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"123456"}' 2>/dev/null || true)")"
if [[ -z "${WORKER_TOKEN}" ]]; then
  echo "Failed to get worker auth token."
  exit 1
fi

MASTER_AUTH_HEADER="Authorization: Bearer ${MASTER_TOKEN}"
WORKER_AUTH_HEADER="Authorization: Bearer ${WORKER_TOKEN}"

json_get() {
  local path="$1"
  "${PYTHON_BIN}" -c 'import json,sys; path=sys.argv[1].split(".") if sys.argv[1] else []; data=json.load(sys.stdin); cur=data; 
for p in path:
    if isinstance(cur, dict) and p in cur:
        cur=cur[p]
    else:
        cur=""
        break
print(json.dumps(cur) if isinstance(cur,(dict,list)) else cur)' "${path}"
}

json_code() {
  "${PYTHON_BIN}" -c 'import json,sys; 
try:
    data=json.load(sys.stdin)
    print(data.get("code",""))
except Exception:
    print("")'
}

RUN_SUFFIX="$(date +%s)_$$"
export RUN_SUFFIX

TEMPLATE_ID="tpl_${RUN_SUFFIX}"
export TEMPLATE_ID
TEMPLATE_JSON="${TMP_DIR}/template.json"
"${PYTHON_BIN}" - <<'PY' > "${TEMPLATE_JSON}"
import json,os
suffix = os.environ["RUN_SUFFIX"]
template_id = os.environ["TEMPLATE_ID"]
payload = {
  "template_id": template_id,
  "name": f"tpl-{suffix}",
  "description": "template for dag/template exec check",
  "task_json_template": {
    "name": f"tpl-{suffix}",
    "tasks": [
      {"id": "tpl-step", "name": "tpl-step", "exec_type": "Shell", "exec_command": f"echo template-{suffix}"}
    ]
  },
  "schema": {}
}
print(json.dumps(payload))
PY

echo "Creating template: ${TEMPLATE_ID}"
template_resp="$(curl -s -X POST "${MASTER_URL}/template" \
  -H "Content-Type: application/json" \
  -H "${MASTER_AUTH_HEADER}" \
  -d @"${TEMPLATE_JSON}" || true)"
if [[ "$(json_code <<<"${template_resp}")" != "0" ]]; then
  echo "Template create failed: ${template_resp}"
  exit 1
fi

PARENT_DAG_JSON="${TMP_DIR}/parent_dag.json"
"${PYTHON_BIN}" - <<'PY' > "${PARENT_DAG_JSON}"
import json,os
suffix = os.environ["RUN_SUFFIX"]
template_id = os.environ["TEMPLATE_ID"]
child_dag = {
  "name": f"child-dag-{suffix}",
  "tasks": [
    {"id": "child-step", "name": "child-step", "exec_type": "Shell", "exec_command": f"echo child-dag-{suffix}"}
  ]
}
payload = {
  "name": f"parent-{suffix}",
  "config": {"name": f"parent-{suffix}"},
  "tasks": [
    {"id": "child-dag", "name": "child-dag", "exec_type": "Dag", "exec_params": {"dag_json": child_dag}},
    {"id": "child-template", "name": "child-template", "exec_type": "Template", "exec_params": {"template_id": template_id, "template_params_json": {}}}
  ]
}
print(json.dumps(payload))
PY

echo "Submitting parent DAG (exec_type Dag/Template)..."
run_resp="$(curl -s -X POST "${MASTER_URL}/api/dag/run_async" \
  -H "Content-Type: application/json" \
  -H "${MASTER_AUTH_HEADER}" \
  -d @"${PARENT_DAG_JSON}" || true)"
if [[ "$(json_code <<<"${run_resp}")" != "0" ]]; then
  echo "run_async failed: ${run_resp}"
  exit 1
fi

PARENT_RUN_ID="$(json_get "data.run_id" <<<"${run_resp}")"
if [[ -z "${PARENT_RUN_ID}" ]]; then
  echo "Parent run_id not found."
  exit 1
fi
echo "Parent run_id: ${PARENT_RUN_ID}"

wait_for_task_runs_done() {
  local run_id="$1"
  local retries="${2:-30}"
  local delay="${3:-0.5}"
  local payload=""
  for _ in $(seq 1 "${retries}"); do
    payload="$(curl -s "${MASTER_URL}/api/dag/task_runs?run_id=${run_id}" \
      -H "${MASTER_AUTH_HEADER}" 2>/dev/null || true)"
    if [[ -n "${payload}" ]]; then
      done="$("${PYTHON_BIN}" -c 'import json,sys; data=json.load(sys.stdin); items=data.get("data",{}).get("items",[]); 
if not items:
    print("0"); sys.exit(0)
for item in items:
    status=item.get("status")
    if status in (0,1):
        print("0"); break
else:
    print("1")' <<<"${payload}" || echo "0")"
      if [[ "${done}" == "1" ]]; then
        echo "${payload}"
        return 0
      fi
    fi
    sleep "${delay}"
  done
  echo "${payload}"
  return 1
}

echo "Waiting for parent tasks to finish..."
if ! TASK_RUNS_PAYLOAD="$(wait_for_task_runs_done "${PARENT_RUN_ID}" 40 0.5)"; then
  echo "Timed out waiting for parent tasks to finish."
  exit 1
fi

echo "Looking for child runs with parent_run_id/parent_task_id..."
dag_runs_payload="$(curl -s "${MASTER_URL}/api/dag/runs?limit=50" \
  -H "${MASTER_AUTH_HEADER}" 2>/dev/null || true)"

set +e
child_map="$("${PYTHON_BIN}" -c 'import json,sys; parent_run=sys.argv[1]; data=json.load(sys.stdin); items=data.get("data",{}).get("items",[]); 
targets={"child-dag": None, "child-template": None}
for item in items:
    raw=item.get("dag_json") or ""
    if not isinstance(raw,str) or not raw:
        continue
    try:
        j=json.loads(raw)
    except Exception:
        continue
    cfg=j.get("config") or {}
    if cfg.get("parent_run_id") == parent_run:
        t=cfg.get("parent_task_id")
        if t in targets and not targets[t]:
            targets[t]=item.get("run_id")
if not all(targets.values()):
    print(json.dumps({"error": targets})); sys.exit(2)
print(json.dumps(targets))' "${PARENT_RUN_ID}" <<<"${dag_runs_payload}")"

child_status=$?
set -e
if [[ "${child_status}" != "0" ]]; then
  echo "Child run lookup failed: ${child_map}"
  echo "Parent task_runs:"
  echo "${TASK_RUNS_PAYLOAD}"
  echo "Recent master log:"
  tail -n 120 "${MASTER_DIR}/master.log" || true
  exit 1
fi
echo "Child runs: ${child_map}"

WORKER_DAG_JSON="${TMP_DIR}/worker_dag.json"
"${PYTHON_BIN}" - <<'PY' > "${WORKER_DAG_JSON}"
import json,os
suffix = os.environ["RUN_SUFFIX"]
payload = {
  "type": "dag",
  "payload": {
    "name": f"worker-dag-{suffix}",
    "tasks": [
      {"id": "wk-step", "name": "wk-step", "exec_type": "Shell", "exec_command": f"echo worker-dag-{suffix}"}
    ]
  }
}
print(json.dumps(payload))
PY

echo "Dispatching worker_execute (type=dag)..."
worker_exec_resp="$(curl -s -X POST "${WORKER_URL}/api/worker/execute" \
  -H "Content-Type: application/json" \
  -d @"${WORKER_DAG_JSON}" || true)"
if [[ "$(json_code <<<"${worker_exec_resp}")" != "0" ]]; then
  echo "worker_execute failed: ${worker_exec_resp}"
  exit 1
fi

WORKER_RUN_ID="$(json_get "data.run_id" <<<"${worker_exec_resp}")"
if [[ -z "${WORKER_RUN_ID}" ]]; then
  echo "Worker run_id not found."
  exit 1
fi
echo "Worker run_id: ${WORKER_RUN_ID}"

worker_runs_payload="$(curl -s "${WORKER_URL}/api/dag/runs?run_id=${WORKER_RUN_ID}" \
  -H "${WORKER_AUTH_HEADER}" 2>/dev/null || true)"
worker_count="$("${PYTHON_BIN}" -c 'import json,sys; data=json.load(sys.stdin); items=data.get("data", {}).get("items", []); print(len(items))' <<<"${worker_runs_payload}" || echo "0")"
if [[ "${worker_count}" == "0" ]]; then
  echo "Worker dag run not found: ${worker_runs_payload}"
  exit 1
fi

echo "Done. Logs at: ${TMP_DIR}"
