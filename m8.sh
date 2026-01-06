#!/usr/bin/env bash

# TaskHub DAG 回归测试脚本（M8/M8.2）
# 使用方式：
#   chmod +x test_m8.sh
#   ./test_m8.sh
#
# 环境变量：
#   BASE_URL 可自定义（默认 http://localhost:8082）

BASE_URL=${BASE_URL:-http://localhost:8082}
ENDPOINT="$BASE_URL/api/dag/run"
USER="${USER:-admin}"
PASS="${PASS:-123456}"
TOKEN=""
AUTH_HEADER=""

echo "Using endpoint: $ENDPOINT"
echo

# 小工具：如果有 jq 就美化输出，否则直接打印
pretty() {
  if command -v jq >/dev/null 2>&1; then
    jq .
  else
    cat
  fi
}

login() {
  local resp
  resp=$(curl -s -X POST "$BASE_URL/api/login" \
    -H "Content-Type: application/json" \
    -d "{\"username\":\"$USER\",\"password\":\"$PASS\"}")

  if command -v jq >/dev/null 2>&1; then
    TOKEN=$(echo "$resp" | jq -r '.data.token')
  else
    TOKEN=$(python3 -c 'import json,sys; data=json.load(sys.stdin); print(data.get("data",{}).get("token",""))' <<<"$resp")
  fi

  if [[ -z "${TOKEN:-}" || "$TOKEN" == "null" ]]; then
    echo "ERROR: login failed, no token in response" >&2
    echo "$resp" >&2
    exit 1
  fi
  AUTH_HEADER="Authorization: Bearer $TOKEN"
}

run_case() {
  local name="$1"
  echo "=============================="
  echo "▶ $name"
  echo "------------------------------"
  shift
  # 其余参数传给 curl
  curl -s "$@" -H "$AUTH_HEADER" | pretty
  echo
  echo
}

login

# # 1. 线性 DAG：A -> B -> C（全部成功）
# run_case "CASE 1: linear DAG A -> B -> C (all success)" \
#   -X POST "$ENDPOINT" \
#   -H "Content-Type: application/json" \
#   -d @- <<'JSON'
# {
#   "config": {
#     "fail_policy": "SkipDownstream",
#     "max_parallel": 4
#   },
#   "tasks": [
#     { "id": "A", "deps": [],       "exec_command": "taskA_handler" },
#     { "id": "B", "deps": ["A"],    "exec_command": "taskB_handler" },
#     { "id": "C", "deps": ["B"],    "exec_command": "taskC_handler" }
#   ]
# }
# JSON


# # 2. 多根节点并发：A, B 都是 root
# run_case "CASE 2: parallel roots A, B (no deps)" \
#   -X POST "$ENDPOINT" \
#   -H "Content-Type: application/json" \
#   -d @- <<'JSON'
# {
#   "config": {
#     "fail_policy": "SkipDownstream",
#     "max_parallel": 4
#   },
#   "tasks": [
#     { "id": "A", "deps": [], "exec_command": "taskA_handler" },
#     { "id": "B", "deps": [], "exec_command": "taskB_handler" }
#   ]
# }
# JSON


# # 3. 菱形 DAG：A -> {B, C} -> D（全部成功）
# run_case "CASE 3: diamond DAG A -> {B, C} -> D (all success)" \
#   -X POST "$ENDPOINT" \
#   -H "Content-Type: application/json" \
#   -d @- <<'JSON'
# {
#   "config": {
#     "fail_policy": "SkipDownstream",
#     "max_parallel": 4
#   },
#   "tasks": [
#     { "id": "A", "deps": [],       "exec_command": "taskA_handler" },
#     { "id": "B", "deps": ["A"],    "exec_command": "taskB_handler" },
#     { "id": "C", "deps": ["A"],    "exec_command": "taskC_handler" },
#     { "id": "D", "deps": ["B","C"],"exec_command": "taskD_handler" }
#   ]
# }
# JSON


# # 4. SkipDownstream：B 失败，D 被跳过
# #   A -> {B_fail, C} -> D
# run_case "CASE 4: SkipDownstream - B fails, downstream D should be skipped" \
#   -X POST "$ENDPOINT" \
#   -H "Content-Type: application/json" \
#   -d @- <<'JSON'
# {
#   "config": {
#     "fail_policy": "SkipDownstream",
#     "max_parallel": 4
#   },
#   "tasks": [
#     { "id": "A", "deps": [],        "exec_command": "taskA_handler" },
#     { "id": "B", "deps": ["A"],     "exec_command": "taskB_fail_handler" },
#     { "id": "C", "deps": ["A"],     "exec_command": "taskC_handler" },
#     { "id": "D", "deps": ["B","C"], "exec_command": "taskD_handler" }
#   ]
# }
# JSON


# # 5. FailFast：A、B 为 root，B 失败后立即停
# run_case "CASE 5: FailFast - A, B roots, B fails -> DAG should stop early" \
#   -X POST "$ENDPOINT" \
#   -H "Content-Type: application/json" \
#   -d @- <<'JSON'
# {
#   "config": {
#     "fail_policy": "FailFast",
#     "max_parallel": 4
#   },
#   "tasks": [
#     { "id": "A", "deps": [], "exec_command": "taskA_handler" },
#     { "id": "B", "deps": [], "exec_command": "taskB_fail_handler" }
#   ]
# }
# JSON


# # 6. Builder 校验：依赖不存在（Bad dependency）
# run_case "CASE 6: Builder validation - dependency refers to unknown node" \
#   -X POST "$ENDPOINT" \
#   -H "Content-Type: application/json" \
#   -d @- <<'JSON'
# {
#   "config": {
#     "fail_policy": "SkipDownstream",
#     "max_parallel": 4
#   },
#   "tasks": [
#     { "id": "A", "deps": [],    "exec_command": "taskA_handler" },
#     { "id": "B", "deps": ["X"], "exec_command": "taskB_handler" }
#   ]
# }
# JSON


# # 7. Builder 校验：有环 A -> B -> C -> A
# run_case "CASE 7: Builder validation - cycle A -> B -> C -> A" \
#   -X POST "$ENDPOINT" \
#   -H "Content-Type: application/json" \
#   -d @- <<'JSON'
# {
#   "config": {
#     "fail_policy": "SkipDownstream",
#     "max_parallel": 4
#   },
#   "tasks": [
#     { "id": "A", "deps": ["C"], "exec_command": "taskA_handler" },
#     { "id": "B", "deps": ["A"], "exec_command": "taskB_handler" },
#     { "id": "C", "deps": ["B"], "exec_command": "taskC_handler" }
#   ]
# }
# JSON
# #8.Shell测试
# run_case "CASE 8: Shell" \
#   -X POST "$ENDPOINT" \
#   -H "Content-Type: application/json" \
#   -d @- <<'JSON'
# {
#   "config": { "fail_policy": "SkipDownstream", "max_parallel": 2 },
#   "tasks": [
#     { "id": "echo_ok", "deps": [], "exec_type": "Shell", "exec_command": "echo hello && exit 0" },
#     { "id": "echo_fail", "deps": [], "exec_type": "Shell", "exec_command": "echo bye && exit 1" }
#   ]
# }
# JSON

# 9. Timeout：Shell 任务 sleep 5s，但设置超时 1s（请根据实际字段名调整 timeout_* 字段）
#    ⚠ 注意：这里假定 DAG JSON 中支持每个 task 级别的 timeout 字段，比如 timeout_ms。
#    如果你在 DagTaskSpec/TaskConfig 里用的是其它字段名（如 timeout_sec），请对应修改。
run_case "CASE 9: Timeout - shell sleep 5s but timeout 1s (expect Timeout)" \
  -X POST "$ENDPOINT" \
  -H "Content-Type: application/json" \
  -d @- <<'JSON'
{
  "config": {
    "fail_policy": "SkipDownstream",
    "max_parallel": 1
  },
  "tasks": [
    {
      "id": "timeout_shell",
      "deps": [],
      "exec_type": "Shell",
      "exec_command": "sleep 10",
      "timeout_ms": 3111
    }
  ]
}
JSON


# 10. Retry：Shell 任务总是以非 0 退出，配置重试次数，看日志中是否多次执行
#     ⚠ 同样，这里的 max_retries / retry_delay_ms 需要与你 TaskConfig 的字段名保持一致。
run_case "CASE 10: Retry - shell always fails, expect multiple attempts" \
  -X POST "$ENDPOINT" \
  -H "Content-Type: application/json" \
  -d @- <<'JSON'
{
  "config": {
    "fail_policy": "SkipDownstream",
    "max_parallel": 1
  },
  "tasks": [
    {
      "id": "retry_shell",
      "deps": [],
      "exec_type": "Shell",
      "exec_command": "echo fail && exit 1",

      "retry_count": 3,
      "retry_delay_ms": 500
    }
  ]
}
JSON

echo "=============================="
echo "全部用例已完成。"
echo "如果上面有 jq 输出的 summary / error message，结合日志看一眼就能验证 M8 的完整行为。"
