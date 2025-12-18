#!/usr/bin/env bash
set -euo pipefail

BASE="${BASE:-http://localhost:8082}"

req() {
  local method="$1"; shift
  local url="$1"; shift
  local body="${1:-}"
  if [[ "$method" == "GET" ]]; then
    curl -sS "$url"
  else
    curl -sS -X "$method" "$url" -H "Content-Type: application/json" -d "$body"
  fi
}

jget() {
  python3 -c 'import json,sys
p=sys.argv[1]
obj=json.load(sys.stdin)
cur=obj
for part in p.split("."):
    if not part:
        continue
    if part.endswith("]"):
        name,idx=part[:-1].split("[")
        if name:
            cur=cur[name]
        cur=cur[int(idx)]
    else:
        cur=cur.get(part)
print(cur if cur is not None else "")
' "$1"
}

count_logs() {
  local tid="$1"
  local from=1
  local limit=200
  local total=0

  while true; do
    local r
    r="$(req GET "$BASE/api/tasks/logs?task_id=$tid&from=$from&limit=$limit")"

    # records 长度
    local n
    n="$(echo "$r" | python3 -c 'import json,sys; o=json.load(sys.stdin); print(len(o.get("records", [])))')"

    # next_from
    local next_from
    next_from="$(echo "$r" | python3 -c 'import json,sys; o=json.load(sys.stdin); v=o.get("next_from"); print("" if v is None else v)')"

    total=$((total + n))

    # 没有更多了：records 为空 或 next_from 为空/不变
    if [[ "$n" -eq 0 ]]; then
      break
    fi
    if [[ -z "$next_from" || "$next_from" == "$from" ]]; then
      break
    fi

    from="$next_from"
  done

  echo "$total"
}

last_log_ts() {
  local tid="$1"
  local r
  r="$(req GET "$BASE/api/tasks/logs?task_id=$tid&from=1&limit=200")"
  echo "$r" | python3 -c '
import json,sys
o=json.load(sys.stdin)
recs=o.get("records",[])
print(recs[-1].get("ts_ms",0) if recs else 0)
'
}
expect_in_array() {
  local name="$1"
  local arr_json="$2"
  local want="$3"
  python3 -c 'import json,sys
name=sys.argv[1]
arr=json.loads(sys.argv[2])
want=sys.argv[3]
if want not in arr:
    print(f"ERROR: expected {want} in {name}, got {arr}")
    sys.exit(1)
print(f"[OK] {name} contains {want}")
' "$name" "$arr_json" "$want"
}

expect_not_in_array() {
  local name="$1"
  local arr_json="$2"
  local notwant="$3"
  python3 -c 'import json,sys
name=sys.argv[1]
arr=json.loads(sys.argv[2])
notwant=sys.argv[3]
if notwant in arr:
    print(f"ERROR: expected {notwant} NOT in {name}, got {arr}")
    sys.exit(1)
print(f"[OK] {name} does not contain {notwant}")
' "$name" "$arr_json" "$notwant"
}

echo "BASE=$BASE"
echo

# -----------------------------
# CASE 1: all success
# -----------------------------
echo "=============================="
echo "CASE 1) A/B/C/D all success"
echo "=============================="

# 清理旧日志（如果你没有清理接口就忽略；这里只做提示）
echo "INFO: This test does NOT delete historical logs; it validates existence and count deltas when needed."

BODY_OK="$(cat <<'JSON'
{
  "config": { "fail_policy": "SkipDownstream", "max_parallel": 2 },
  "tasks": [
    { "id": "A", "exec_type": "Shell", "exec_params": { "inner.exec_command": "echo DAG_A_OK", "inner.exec_type": "Shell" }, "exec_command": "echo DAG_A_OK" },
    { "id": "B", "deps": ["A"], "exec_type": "Shell", "exec_command": "echo DAG_B_OK" },
    { "id": "C", "deps": ["A"], "exec_type": "Shell", "exec_command": "echo DAG_C_OK" },
    { "id": "D", "deps": ["B","C"], "exec_type": "Shell", "exec_command": "echo DAG_D_OK" }
  ]
}
JSON
)"

R1="$(req POST "$BASE/api/dag/run" "$BODY_OK")"
echo "$R1" | python3 -m json.tool

# summary 校验
TOTAL="$(echo "$R1" | jget "summary.total")"
if [[ "$TOTAL" != "4" ]]; then
  echo "ERROR: expected summary.total=4, got $TOTAL"
  exit 1
fi
echo "[OK] summary.total=4"

# success 应包含 A/B/C/D
SUCCESS_ARR="$(echo "$R1" | python3 -c 'import json,sys; print(json.dumps(json.load(sys.stdin)["summary"]["success"]))')"
expect_in_array "summary.success" "$SUCCESS_ARR" "A"
expect_in_array "summary.success" "$SUCCESS_ARR" "B"
expect_in_array "summary.success" "$SUCCESS_ARR" "C"
expect_in_array "summary.success" "$SUCCESS_ARR" "D"

# 每个 task_id 必须有日志
for tid in A B C D; do
  c="$(count_logs "$tid")"
  if [[ "$c" -le 0 ]]; then
    echo "ERROR: expected logs for task_id=$tid, got count=$c"
    exit 1
  fi
  echo "[OK] logs exist for task_id=$tid (count=$c)"
done

echo

# -----------------------------
# CASE 2: B fails, SkipDownstream
# -----------------------------
echo "=============================="
echo "CASE 2) B fails, SkipDownstream; expect D skipped and NOT executed"
echo "=============================="

# 运行前先记录 A/B/C/D 日志条数（避免历史残留导致误判）
BEFORE_A="$(count_logs A)"
BEFORE_B="$(count_logs B)"
BEFORE_C="$(count_logs C)"
BEFORE_D="$(count_logs D)"

BEFORE_D_TS="$(last_log_ts D)"

echo "Before logs: A=$BEFORE_A B=$BEFORE_B C=$BEFORE_C D=$BEFORE_D"

BODY_FAIL="$(cat <<'JSON'
{
  "config": { "fail_policy": "SkipDownstream", "max_parallel": 2 },
  "tasks": [
    { "id": "A", "exec_type": "Shell", "exec_command": "echo DAG_A_OK" },
    { "id": "B", "deps": ["A"], "exec_type": "Shell", "exec_command": "echo DAG_B_FAIL && exit 2" },
    { "id": "C", "deps": ["A"], "exec_type": "Shell", "exec_command": "echo DAG_C_OK" },
    { "id": "D", "deps": ["B","C"], "exec_type": "Shell", "exec_command": "echo DAG_D_SHOULD_NOT_RUN" }
  ]
}
JSON
)"

R2="$(req POST "$BASE/api/dag/run" "$BODY_FAIL")"
echo "$R2" | python3 -m json.tool

# summary.total 必须=4
TOTAL2="$(echo "$R2" | jget "summary.total")"
if [[ "$TOTAL2" != "4" ]]; then
  echo "ERROR: expected summary.total=4, got $TOTAL2"
  exit 1
fi
echo "[OK] summary.total=4"

FAILED_ARR="$(echo "$R2" | python3 -c 'import json,sys; print(json.dumps(json.load(sys.stdin)["summary"]["failed"]))')"
SUCCESS2_ARR="$(echo "$R2" | python3 -c 'import json,sys; print(json.dumps(json.load(sys.stdin)["summary"]["success"]))')"
SKIPPED_ARR="$(echo "$R2" | python3 -c 'import json,sys; print(json.dumps(json.load(sys.stdin)["summary"]["skipped"]))')"

# 期望：B failed；A,C success；D skipped
expect_in_array "summary.failed"  "$FAILED_ARR"  "B"
expect_in_array "summary.success" "$SUCCESS2_ARR" "A"
expect_in_array "summary.success" "$SUCCESS2_ARR" "C"
expect_in_array "summary.skipped" "$SKIPPED_ARR" "D"

# D 不应被执行：用“日志增量”为准（如果你实现了 Skipped 也写日志，这里就会增量>0）
AFTER_A="$(count_logs A)"
AFTER_B="$(count_logs B)"
AFTER_C="$(count_logs C)"
AFTER_D="$(count_logs D)"

DELTA_A=$((AFTER_A - BEFORE_A))
DELTA_B=$((AFTER_B - BEFORE_B))
DELTA_C=$((AFTER_C - BEFORE_C))
DELTA_D=$((AFTER_D - BEFORE_D))

echo "After logs : A=$AFTER_A B=$AFTER_B C=$AFTER_C D=$AFTER_D"
echo "Delta logs : A=+$DELTA_A B=+$DELTA_B C=+$DELTA_C D=+$DELTA_D"

# A/B/C 至少应产生新日志（增量>0）
if [[ "$DELTA_A" -le 0 ]]; then echo "ERROR: expected A log delta > 0"; exit 1; fi
if [[ "$DELTA_B" -le 0 ]]; then echo "ERROR: expected B log delta > 0"; exit 1; fi
if [[ "$DELTA_C" -le 0 ]]; then echo "ERROR: expected C log delta > 0"; exit 1; fi
echo "[OK] A/B/C produced new logs"

# D 应该不执行：理想情况 delta=0
# 如果你决定“写一条 Skipped 日志”，那就把这里改成 <=1 或检查 message 包含 "Skipped"
if [[ "$DELTA_D" -ne 0 ]]; then
  echo "ERROR: expected D log delta == 0 (not executed), got +$DELTA_D"
  echo "HINT: Your scheduler likely still executed D, or you are logging skipped nodes as normal execution."
  exit 1
fi
echo "[OK] D produced no new logs (not executed)"

AFTER_D_TS="$(last_log_ts D)"
if [[ "$AFTER_D_TS" != "$BEFORE_D_TS" ]]; then
  echo "ERROR: expected D last ts unchanged, but changed"
  exit 1
fi
echo "[OK] D last log ts unchanged (not executed)"
echo

echo "ALL DAG ABCD tests passed."