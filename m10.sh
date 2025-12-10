#!/usr/bin/env bash
set -e

ENDPOINT="http://localhost:8082"
echo "Using endpoint: $ENDPOINT"

# ----------------------------------------
# Helper
run_post() {
  local title="$1"
  shift
  echo
  echo "=============================="
  echo "▶ $title"
  echo "------------------------------"
  curl -s -X POST "$@" | jq
}

# ----------------------------------------
# 1. Create SingleTask cron
run_post "Create Cron SingleTask (every minute)" \
  "$ENDPOINT/api/cron/jobs" \
  -H "Content-Type: application/json" \
  -d @- <<'JSON'
{
  "name": "cron_demo_single",
  "spec": "*/1 * * * *",
  "target_type": "SingleTask",
  "task": {
    "id": "cron_test_single",
    "name": "cron_test_single",
    "exec_type": "Shell",
    "exec_command": "echo [Cron SingleTask] Hello",
    "priority": "Normal",
    "timeout_ms": 0,
    "retry_count": 0
  }
}
JSON


# ----------------------------------------
# 2. Create DAG cron
run_post "Create Cron DAG (A -> B) (every minute)" \
  "$ENDPOINT/api/cron/jobs" \
  -H "Content-Type: application/json" \
  -d @- <<'JSON'
{
  "name": "cron_demo_dag",
  "spec": "*/1 * * * *",
  "target_type": "Dag",
  "dag": {
    "config": {
      "fail_policy": "SkipDownstream",
      "max_parallel": 1
    },
    "tasks": [
      {
        "id": "CronA",
        "deps": [],
        "exec_type": "Shell",
        "exec_command": "echo [Cron DAG] A"
      },
      {
        "id": "CronB",
        "deps": ["CronA"],
        "exec_type": "Shell",
        "exec_command": "echo [Cron DAG] B"
      }
    ]
  }
}
JSON


# ----------------------------------------
# 3. List cron jobs
echo
echo "=============================="
echo "▶ List cron jobs"
echo "------------------------------"
curl -s "$ENDPOINT/api/cron/jobs" | jq


echo
echo "=============================="
echo "全部用例已完成。"
echo "等待到分钟边界，后台日志应当看到："
echo "  CronScheduler::trigger job ... "
echo "  [Cron SingleTask] Hello"
echo "  [Cron DAG] A"
echo "  [Cron DAG] B"
echo