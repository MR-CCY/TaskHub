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

run_case() {
  local name="$1"
  echo "=============================="
  echo "▶ $name"
  echo "------------------------------"
  shift
  # 其余参数传给 curl
  curl -s "$@" | pretty
  echo
  echo
}

# 1. 线性 DAG：A -> B -> C（全部成功）
run_case "CASE 1: linear DAG A -> B -> C (all success)" \
  -X POST "$ENDPOINT" \
  -H "Content-Type: application/json" \
  -d @- <<'JSON'
{
  "config": {
    "fail_policy": "SkipDownstream",
    "max_parallel": 4
  },
  "tasks": [
    { "id": "A", "deps": [],       "exec_command": "taskA_handler" },
    { "id": "B", "deps": ["A"],    "exec_command": "taskB_handler" },
    { "id": "C", "deps": ["B"],    "exec_command": "taskC_handler" }
  ]
}
JSON


# 2. 多根节点并发：A, B 都是 root
run_case "CASE 2: parallel roots A, B (no deps)" \
  -X POST "$ENDPOINT" \
  -H "Content-Type: application/json" \
  -d @- <<'JSON'
{
  "config": {
    "fail_policy": "SkipDownstream",
    "max_parallel": 4
  },
  "tasks": [
    { "id": "A", "deps": [], "exec_command": "taskA_handler" },
    { "id": "B", "deps": [], "exec_command": "taskB_handler" }
  ]
}
JSON


# 3. 菱形 DAG：A -> {B, C} -> D（全部成功）
run_case "CASE 3: diamond DAG A -> {B, C} -> D (all success)" \
  -X POST "$ENDPOINT" \
  -H "Content-Type: application/json" \
  -d @- <<'JSON'
{
  "config": {
    "fail_policy": "SkipDownstream",
    "max_parallel": 4
  },
  "tasks": [
    { "id": "A", "deps": [],       "exec_command": "taskA_handler" },
    { "id": "B", "deps": ["A"],    "exec_command": "taskB_handler" },
    { "id": "C", "deps": ["A"],    "exec_command": "taskC_handler" },
    { "id": "D", "deps": ["B","C"],"exec_command": "taskD_handler" }
  ]
}
JSON


# 4. SkipDownstream：B 失败，D 被跳过
#   A -> {B_fail, C} -> D
run_case "CASE 4: SkipDownstream - B fails, downstream D should be skipped" \
  -X POST "$ENDPOINT" \
  -H "Content-Type: application/json" \
  -d @- <<'JSON'
{
  "config": {
    "fail_policy": "SkipDownstream",
    "max_parallel": 4
  },
  "tasks": [
    { "id": "A", "deps": [],        "exec_command": "taskA_handler" },
    { "id": "B", "deps": ["A"],     "exec_command": "taskB_fail_handler" },
    { "id": "C", "deps": ["A"],     "exec_command": "taskC_handler" },
    { "id": "D", "deps": ["B","C"], "exec_command": "taskD_handler" }
  ]
}
JSON


# 5. FailFast：A、B 为 root，B 失败后立即停
run_case "CASE 5: FailFast - A, B roots, B fails -> DAG should stop early" \
  -X POST "$ENDPOINT" \
  -H "Content-Type: application/json" \
  -d @- <<'JSON'
{
  "config": {
    "fail_policy": "FailFast",
    "max_parallel": 4
  },
  "tasks": [
    { "id": "A", "deps": [], "exec_command": "taskA_handler" },
    { "id": "B", "deps": [], "exec_command": "taskB_fail_handler" }
  ]
}
JSON


# 6. Builder 校验：依赖不存在（Bad dependency）
run_case "CASE 6: Builder validation - dependency refers to unknown node" \
  -X POST "$ENDPOINT" \
  -H "Content-Type: application/json" \
  -d @- <<'JSON'
{
  "config": {
    "fail_policy": "SkipDownstream",
    "max_parallel": 4
  },
  "tasks": [
    { "id": "A", "deps": [],    "exec_command": "taskA_handler" },
    { "id": "B", "deps": ["X"], "exec_command": "taskB_handler" }
  ]
}
JSON


# 7. Builder 校验：有环 A -> B -> C -> A
run_case "CASE 7: Builder validation - cycle A -> B -> C -> A" \
  -X POST "$ENDPOINT" \
  -H "Content-Type: application/json" \
  -d @- <<'JSON'
{
  "config": {
    "fail_policy": "SkipDownstream",
    "max_parallel": 4
  },
  "tasks": [
    { "id": "A", "deps": ["C"], "exec_command": "taskA_handler" },
    { "id": "B", "deps": ["A"], "exec_command": "taskB_handler" },
    { "id": "C", "deps": ["B"], "exec_command": "taskC_handler" }
  ]
}
JSON


echo "=============================="
echo "全部用例已完成。"
echo "如果上面有 jq 输出的 summary / error message，结合日志看一眼就能验证 M8 的完整行为。"