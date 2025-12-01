#!/usr/bin/env bash
set -euo pipefail

echo ">>> RUNNING SCRIPT: $0"

API_BASE="http://localhost:8082"
USER="admin"
PASS="123456"

# 确保 jq 存在
if ! command -v jq >/dev/null 2>&1; then
  echo "ERROR: jq is required but not installed." >&2
  exit 1
fi

login() {
  echo "=> 登录获取 TOKEN ..." >&2
  local resp
  resp=$(curl -s -X POST "$API_BASE/api/login" \
    -H "Content-Type: application/json" \
    -d "{\"username\":\"$USER\",\"password\":\"$PASS\"}")

  echo "$resp" | jq >&2

  TOKEN=$(echo "$resp" | jq -r '.data.token')
  if [[ -z "${TOKEN:-}" || "$TOKEN" == "null" ]]; then
    echo "ERROR: login failed, no token in response" >&2
    exit 1
  fi

  echo "=> TOKEN = $TOKEN" >&2
}

# 创建任务：返回 task id（stdout），日志走 stderr
create_task() {
  local name="$1"
  local cmd="$2"
  local max_retries="$3"
  local timeout_sec="$4"

  echo >&2
  echo "=> 创建任务: name=$name max_retries=$max_retries timeout_sec=$timeout_sec" >&2

  local payload
  payload=$(jq -n \
    --arg name "$name" \
    --arg cmd "$cmd" \
    --argjson max_retries "$max_retries" \
    --argjson timeout_sec "$timeout_sec" '
      {
        name: $name,
        type: 1,
        params: {
          cmd: $cmd,
          max_retries: $max_retries,
          timeout_sec: $timeout_sec
        }
      }')

  echo "Payload:" >&2
  echo "$payload" | jq >&2

  local resp
  resp=$(curl -s -X POST "$API_BASE/api/tasks" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "$payload")

  echo "Response:" >&2
  echo "$resp" | jq >&2

  local id
  id=$(echo "$resp" | jq -r '.data.id')

  if [[ -z "$id" || "$id" == "null" ]]; then
    echo "ERROR: create_task failed, no id in response" >&2
    exit 1
  fi

  echo "=> 新任务 ID = $id" >&2

  # ★★★ 真正返回值：只输出 ID 到 stdout ★★★
  echo "$id"
}

# 轮询任务状态直到结束
wait_task() {
  local id="$1"
  echo >&2
  echo "=> 轮询任务 $id 状态..." >&2

  while true; do
    local json
    json=$(curl -s "$API_BASE/api/tasks/$id" \
      -H "Authorization: Bearer $TOKEN")

    echo "[RAW] $json" >&2

    local status
    status=$(echo "$json" | jq -r '.data.status')
    local exit_code
    exit_code=$(echo "$json" | jq -r '.data.exit_code')

    echo "[$(date +%T)] task $id status=$status exit=$exit_code" >&2

    if [[ "$status" == "success" || "$status" == "failed" || \
          "$status" == "timeout" || "$status" == "canceled" ]]; then
      echo "最终任务信息:" >&2
      echo "$json" | jq >&2
      break
    fi

    sleep 1
  done
}

# 取消任务
cancel_task() {
  local id="$1"
  echo >&2
  echo "=> 发送取消任务请求: $id" >&2

  local resp
  resp=$(curl -s -X POST "$API_BASE/api/tasks/$id/cancel" \
    -H "Authorization: Bearer $TOKEN")

  echo "$resp" | jq >&2
}

# ------------ 测试 1：超时 ------------
test_timeout() {
  echo
  echo "============================"
  echo "TEST 1: 任务超时 (timeout)"
  echo "============================"

  local cmd="sleep 5 && echo done_timeout"

  echo "DEBUG: before create_task" >&2
  local id
  id=$(create_task "TimeoutTest" "$cmd" 0 2)
  echo "DEBUG: after create_task, id=$id" >&2

  wait_task "$id"
}

# ------------ 测试 2：自动重试 ------------
test_retry() {
  echo
  echo "============================"
  echo "TEST 2: 自动重试 (retry)"
  echo "============================"

  # 用文件标记的方式模拟“第一次失败，第二次成功”
  rm -f /tmp/taskhub_retry_demo

  local cmd='bash -c "if [ ! -f /tmp/taskhub_retry_demo ]; then echo first_fail >&2; touch /tmp/taskhub_retry_demo; exit 1; else echo success_retry; rm -f /tmp/taskhub_retry_demo; exit 0; fi"'
  local id
  id=$(create_task "RetryTest" "$cmd" 3 10)
  wait_task "$id"
}

# ------------ 测试 3：取消排队任务 ------------
test_cancel_queued() {
  echo
  echo "============================"
  echo "TEST 3: 取消排队中的任务 (cancel queued)"
  echo "============================"
  echo "!!! 建议 WorkerPool 暂时改成 start(1) 更好观察队列行为" >&2

  local cmd_long="sleep 30 && echo LONG_TASK_DONE"

  # 先提交一个长期任务 T1
  local id1
  id1=$(create_task "LongTask1" "$cmd_long" 0 0)

  # 再提交第二个长期任务 T2（应当排队）
  local id2
  id2=$(create_task "LongTask2" "$cmd_long" 0 0)

  # 取消排队中的 T2
  cancel_task "$id2"

  echo
  echo "=> 等待 T2 最终状态：" >&2
  wait_task "$id2"

  echo
  echo "=> 等待 T1 最终状态：" >&2
  wait_task "$id1"
}

main() {
  login

  echo
  echo "===== 开始 M7 测试 ====="

  test_timeout
  test_retry
  test_cancel_queued

  echo
  echo "===== M7 测试完成 ====="
}

main "$@"