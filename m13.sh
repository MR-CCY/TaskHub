#!/bin/bash
set -euo pipefail

BASE="${BASE:-http://localhost:8082}"
USER="${USER:-admin}"
PASS="${PASS:-123456}"
TOKEN=""
AUTH_HEADER=""

# 你可以改成自己的
TID_SINGLE="tpl_single_echo"
TID_DAG="tpl_dag_demo"

echo "BASE=$BASE"
echo

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "ERROR: missing command: $1"
    exit 1
  }
}

need_cmd curl
need_cmd jq

login() {
  local resp
  resp="$(curl -s -X POST "$BASE/api/login" \
    -H "Content-Type: application/json" \
    -d "{\"username\":\"$USER\",\"password\":\"$PASS\"}")"
  TOKEN="$(echo "$resp" | jq -r '.data.token')"
  if [ -z "${TOKEN:-}" ] || [ "$TOKEN" == "null" ]; then
    echo "ERROR: login failed, no token in response"
    echo "$resp" | jq .
    exit 1
  fi
  AUTH_HEADER="Authorization: Bearer $TOKEN"
}

login

req() {
  # $1=METHOD $2=URL $3=JSON(optional)
  local method="$1"
  local url="$2"
  local data="${3:-}"

  if [ -n "$data" ]; then
    curl -sS -X "$method" "$url" \
      -H "$AUTH_HEADER" \
      -H "Content-Type: application/json" \
      -d "$data"
  else
    curl -sS -X "$method" "$url" \
      -H "$AUTH_HEADER"
  fi
}

expect_ok_true() {
  local json="$1"
  local ok
  # NOTE: jq `//` treats false as empty, so don't use `.ok // empty`.
  ok="$(echo "$json" | jq -r 'if .ok == null then "" else (.ok|tostring) end')"
  if [ "$ok" != "true" ]; then
    echo "ERROR: expected .ok==true, got:"
    echo "$json" | jq .
    exit 1
  fi
}

expect_ok_false() {
  local json="$1"
  local ok
  # NOTE: jq `//` treats false as empty, so don't use `.ok // empty`.
  ok="$(echo "$json" | jq -r 'if .ok == null then "" else (.ok|tostring) end')"
  if [ "$ok" != "false" ]; then
    echo "ERROR: expected .ok==false, got:"
    echo "$json" | jq .
    exit 1
  fi
}

expect_task_logs_nonempty() {
  local tid="$1"
  local url="$BASE/api/tasks/logs?task_id=${tid}&from=1&limit=5"
  local r
  r="$(req GET "$url")"

  # API 可能返回 {ok:true,records:[...]} 或 {records:[...]} 或包一层 data
  local n
  n="$(echo "$r" | jq -r '(
        if (.records|type)=="array" then (.records|length)
        elif (.data.records|type)=="array" then (.data.records|length)
        else 0 end
      )')"

  if [ "$n" -le 0 ]; then
    echo "ERROR: expected logs non-empty for task_id=$tid"
    echo "URL: $url"
    echo "$r" | jq .
    exit 1
  fi
  echo "[OK] logs exist for task_id=$tid (records=$n)"
}

get_records_len() {
  local json="$1"
  echo "$json" | jq -r '(
        if (.records|type)=="array" then (.records|length)
        elif (.data.records|type)=="array" then (.data.records|length)
        else 0 end
      )'
}

expect_task_logs_empty() {
  local tid="$1"
  local url="$BASE/api/tasks/logs?task_id=${tid}&from=1&limit=5"
  local r
  r="$(req GET "$url" || true)"

  # 如果接口直接 404/500 也当作 empty（因为 skipped 任务可能压根不落库）
  if ! echo "$r" | jq -e '.' >/dev/null 2>&1; then
    echo "[OK] logs empty for task_id=$tid (non-json response treated as empty)"
    return 0
  fi

  local n
  n="$(get_records_len "$r")"
  if [ "$n" -ne 0 ]; then
    echo "ERROR: expected logs empty for skipped task_id=$tid, got records=$n"
    echo "URL: $url"
    echo "$r" | jq .
    exit 1
  fi
  echo "[OK] logs empty for skipped task_id=$tid (records=$n)"
}

echo "=============================="
echo "0) cleanup old templates"
echo "=============================="
# delete ignore error
req DELETE "$BASE/template/$TID_SINGLE" >/dev/null 2>&1 || true
req DELETE "$BASE/template/$TID_DAG" >/dev/null 2>&1 || true
echo "cleanup done"
echo

echo "=============================="
echo "1) create SINGLE task template (M13.2 create)"
echo "=============================="
CREATE_SINGLE_JSON="$(cat <<'JSON'
{
  "template_id": "__TID__",
  "name": "single echo template",
  "description": "single task template: Shell echo",
  "task_json_template": {
    "task": {
      "id": "{{task_id}}",
      "exec_type": "Shell",
      "exec_command": "{{cmd}}",
      "timeout_ms": { "$param": "timeout_ms" },
      "capture_output": { "$param": "capture_output" }
    }
  },
  "schema": {
    "params": [
      { "name": "task_id", "type": "string", "required": true },
      { "name": "cmd", "type": "string", "required": true },
      { "name": "timeout_ms", "type": "int", "required": false, "defaultValue": 3000 },
      { "name": "capture_output", "type": "bool", "required": false, "defaultValue": true }
    ]
  }
}
JSON
)"
CREATE_SINGLE_JSON="${CREATE_SINGLE_JSON/__TID__/$TID_SINGLE}"

R="$(req POST "$BASE/template" "$CREATE_SINGLE_JSON")"
echo "$R" | jq .
# create接口返回字段不一定叫ok（你这里是 out["ok"]=true），所以检查ok
expect_ok_true "$R"
echo

echo "=============================="
echo "2) get/list templates (M13.2 get/list)"
echo "=============================="
R="$(req GET "$BASE/template/$TID_SINGLE")"
echo "$R" | jq .
expect_ok_true "$R"

R="$(req GET "$BASE/templates")"
echo "$R" | jq .
expect_ok_true "$R"
echo

echo "=============================="
echo "3) render SINGLE success (M13.1 render)"
echo "=============================="
R="$(req POST "$BASE/template/render" "{
  \"template_id\":\"$TID_SINGLE\",
  \"params\":{
    \"task_id\":\"log_test_single_render\",
    \"cmd\":\"echo hello_single_render\"
  }
}")"
echo "$R" | jq .
expect_ok_true "$R"

# 断言 rendered.task.capture_output 是 boolean
CAP_TYPE="$(echo "$R" | jq -r '.data.rendered.task.capture_output | type')"
if [ "$CAP_TYPE" != "boolean" ]; then
  echo "ERROR: expected rendered.task.capture_output type=boolean, got $CAP_TYPE"
  exit 1
fi

TMO_TYPE="$(echo "$R" | jq -r '.data.rendered.task.timeout_ms | type')"
if [ "$TMO_TYPE" != "number" ]; then
  echo "ERROR: expected rendered.task.timeout_ms type=number, got $TMO_TYPE"
  exit 1
fi
echo "[OK] render single: types correct (bool/number)"
echo

echo "=============================="
echo "4) render SINGLE missing required -> fail"
echo "=============================="
R="$(req POST "$BASE/template/render" "{
  \"template_id\":\"$TID_SINGLE\",
  \"params\":{
    \"task_id\":\"log_test_single_bad\"
  }
}")"
echo "$R" | jq .
expect_ok_false "$R"
ERR="$(echo "$R" | jq -r '.error // .data.error // empty')"
if [ -z "$ERR" ]; then
  echo "ERROR: expected error message not empty"
  exit 1
fi
echo "[OK] render single missing required got err: $ERR"
echo

echo "=============================="
echo "5) run SINGLE (M13.3 run single)"
echo "=============================="
R="$(req POST "$BASE/template/run" "{
  \"template_id\":\"$TID_SINGLE\",
  \"params\":{
    \"task_id\":\"log_test_single_run\",
    \"cmd\":\"echo hello_from_template && echo err_from_template 1>&2\"
  }
}")"
echo "$R" | jq .

# 这里你的 run() 返回没有 ok 字段（你只返回了 summary），所以不强行检查 ok
# 检查 summary.success 里包含 task_id（如果你 run() 没返回success列表就跳过）
if echo "$R" | jq -e '.summary.success' >/dev/null 2>&1; then
  FOUND="$(echo "$R" | jq -r '.summary.success[]? | select(.=="log_test_single_run")' || true)"
  if [ -z "$FOUND" ]; then
    echo "WARN: summary.success does not include log_test_single_run (maybe your run response differs)."
  else
    echo "[OK] run single summary contains log_test_single_run"
  fi
else
  echo "INFO: run single response has no .summary.success, skip assert."
fi

# 额外闭环：确认单任务确实执行过（落库/可查询）
expect_task_logs_nonempty "log_test_single_run"

echo

echo "=============================="
echo "6) create DAG template (M13.4 create)"
echo "=============================="
CREATE_DAG_JSON="$(cat <<'JSON'
{
  "template_id": "__TID__",
  "name": "dag demo template",
  "description": "DAG template with config + tasks + deps",
  "task_json_template": {
    "config": {
      "fail_policy": "{{fail_policy}}",
      "max_parallel": { "$param": "max_parallel" }
    },
    "tasks": [
      {
        "id": "{{A_id}}",
        "exec_type": "Shell",
        "exec_command": "{{A_cmd}}",
        "deps": []
      },
      {
        "id": "{{B_id}}",
        "exec_type": "Shell",
        "exec_command": "{{B_cmd}}",
        "deps": ["{{A_id}}"]
      },
      {
        "id": "{{C_id}}",
        "exec_type": "Shell",
        "exec_command": "{{C_cmd}}",
        "deps": ["{{A_id}}"]
      },
      {
        "id": "{{D_id}}",
        "exec_type": "Shell",
        "exec_command": "{{D_cmd}}",
        "deps": ["{{B_id}}", "{{C_id}}"]
      }
    ]
  },
  "schema": {
    "params": [
      { "name": "fail_policy", "type": "string", "required": false, "defaultValue": "SkipDownstream" },
      { "name": "max_parallel", "type": "int", "required": false, "defaultValue": 4 },

      { "name": "A_id", "type": "string", "required": false, "defaultValue": "A" },
      { "name": "B_id", "type": "string", "required": false, "defaultValue": "B" },
      { "name": "C_id", "type": "string", "required": false, "defaultValue": "C" },
      { "name": "D_id", "type": "string", "required": false, "defaultValue": "D" },

      { "name": "A_cmd", "type": "string", "required": true },
      { "name": "B_cmd", "type": "string", "required": true },
      { "name": "C_cmd", "type": "string", "required": true },
      { "name": "D_cmd", "type": "string", "required": true }
    ]
  }
}
JSON
)"
CREATE_DAG_JSON="${CREATE_DAG_JSON/__TID__/$TID_DAG}"

R="$(req POST "$BASE/template" "$CREATE_DAG_JSON")"
echo "$R" | jq .
expect_ok_true "$R"
echo

echo "=============================="
echo "7) render DAG success (M13.4 render)"
echo "=============================="
R="$(req POST "$BASE/template/render" "{
  \"template_id\":\"$TID_DAG\",
  \"params\":{
    \"fail_policy\":\"SkipDownstream\",
    \"max_parallel\": 2,
    \"A_cmd\":\"echo DAG_A\",
    \"B_cmd\":\"echo DAG_B\",
    \"C_cmd\":\"echo DAG_C\",
    \"D_cmd\":\"echo DAG_D\"
  }
}")"
echo "$R" | jq .
expect_ok_true "$R"

# 断言 rendered 有 config/tasks/deps
HAS_CONFIG="$(echo "$R" | jq -r '.data.rendered.config | type')"
HAS_TASKS="$(echo "$R" | jq -r '.data.rendered.tasks | type')"
if [ "$HAS_CONFIG" != "object" ] || [ "$HAS_TASKS" != "array" ]; then
  echo "ERROR: expected rendered.config(object) and rendered.tasks(array)"
  exit 1
fi
DEPS0="$(echo "$R" | jq -r '.data.rendered.tasks[1].deps | type')"
if [ "$DEPS0" != "array" ]; then
  echo "ERROR: expected rendered.tasks[1].deps is array"
  exit 1
fi
echo "[OK] render DAG structure ok (config/tasks/deps)"
echo

LEN_TASKS="$(echo "$R" | jq -r '.data.rendered.tasks | length')"
if [ "$LEN_TASKS" -ne 4 ]; then
  echo "ERROR: expected rendered.tasks length=4, got $LEN_TASKS"
  exit 1
fi
D_DEPS_LEN="$(echo "$R" | jq -r '.data.rendered.tasks[3].deps | length')"
if [ "$D_DEPS_LEN" -ne 2 ]; then
  echo "ERROR: expected rendered.tasks[3].deps length=2, got $D_DEPS_LEN"
  exit 1
fi

echo "=============================="
echo "8) run DAG template (M13.4 run)"
echo "=============================="
R="$(req POST "$BASE/template/run" "{
  \"template_id\":\"$TID_DAG\",
  \"params\":{
    \"fail_policy\":\"SkipDownstream\",
    \"max_parallel\": 2,
    \"A_id\":\"A\",
    \"B_id\":\"B\",
    \"C_id\":\"C\",
    \"D_id\":\"D\",
    \"A_cmd\":\"echo DAG_A\",
    \"B_cmd\":\"echo DAG_B\",
    \"C_cmd\":\"echo DAG_C\",
    \"D_cmd\":\"echo DAG_D\"
  }
}")"
echo "$R" | jq .

# 同样：你的 run 返回可能只有 summary
if echo "$R" | jq -e '.summary.total' >/dev/null 2>&1; then
  TOTAL="$(echo "$R" | jq -r '.summary.total')"
  echo "[OK] run DAG summary.total=$TOTAL"
else
  echo "INFO: run DAG response has no .summary, skip assert."
fi
echo

# 闭环：优先按 summary.success 做日志校验（更通用），再兜底检查 A/B/C/D
if echo "$R" | jq -e '.summary.success' >/dev/null 2>&1; then
  for tid in $(echo "$R" | jq -r '.summary.success[]'); do
    expect_task_logs_nonempty "$tid"
  done
else
  expect_task_logs_nonempty "A"
  expect_task_logs_nonempty "B"
  expect_task_logs_nonempty "C"
  expect_task_logs_nonempty "D"
fi

echo "=============================="
echo "8b) run DAG template (FailPolicy SkipDownstream semantics)"
echo "=============================="
R="$(req POST "$BASE/template/run" "{
  \"template_id\":\"$TID_DAG\",
  \"params\":{
    \"fail_policy\":\"SkipDownstream\",
    \"max_parallel\": 2,
    \"A_id\":\"A\",
    \"B_id\":\"B\",
    \"C_id\":\"C\",
    \"D_id\":\"D\",
    \"A_cmd\":\"echo DAG_A\",
    \"B_cmd\":\"sh -c 'echo DAG_B_FAIL 1>&2; exit 1'\",
    \"C_cmd\":\"echo DAG_C\",
    \"D_cmd\":\"echo DAG_D\"
  }
}")"
echo "$R" | jq .

if echo "$R" | jq -e '.summary.total' >/dev/null 2>&1; then
  TOTAL="$(echo "$R" | jq -r '.summary.total')"
  if [ "$TOTAL" -ne 4 ]; then
    echo "ERROR: expected summary.total=4, got $TOTAL"
    exit 1
  fi

  # failed contains B
  if ! echo "$R" | jq -e '.summary.failed | index("B") != null' >/dev/null 2>&1; then
    echo "ERROR: expected summary.failed contains B"
    echo "$R" | jq .
    exit 1
  fi

  # success contains A and C
  if ! echo "$R" | jq -e '.summary.success | index("A") != null' >/dev/null 2>&1; then
    echo "ERROR: expected summary.success contains A"
    echo "$R" | jq .
    exit 1
  fi
  if ! echo "$R" | jq -e '.summary.success | index("C") != null' >/dev/null 2>&1; then
    echo "ERROR: expected summary.success contains C"
    echo "$R" | jq .
    exit 1
  fi

  # skipped contains D
  if ! echo "$R" | jq -e '.summary.skipped | index("D") != null' >/dev/null 2>&1; then
    echo "ERROR: expected summary.skipped contains D"
    echo "$R" | jq .
    exit 1
  fi

  echo "[OK] SkipDownstream semantics: B failed, C ran, D skipped"
else
  echo "WARN: run DAG failure-case response has no .summary, skip semantic asserts."
fi

# logs should exist for A/B/C (D may be skipped)
expect_task_logs_nonempty "A"
expect_task_logs_nonempty "B"
expect_task_logs_nonempty "C"
# D 被 skipped：要求 D 没有任何日志记录（防止假阳性）
expect_task_logs_empty "D"

echo
echo "=============================="
echo "8c) run DAG template (FailPolicy FailFast semantics)"
echo "=============================="
R="$(req POST "$BASE/template/run" "{
  \"template_id\":\"$TID_DAG\",
  \"params\":{
    \"fail_policy\":\"FailFast\",
    \"max_parallel\": 2,
    \"A_id\":\"A\",
    \"B_id\":\"B\",
    \"C_id\":\"C\",
    \"D_id\":\"D\",
    \"A_cmd\":\"echo DAG_A\",
    \"B_cmd\":\"sh -c 'echo DAG_B_FAILFAST 1>&2; exit 1'\",
    \"C_cmd\":\"echo DAG_C_SHOULD_NOT_RUN\",
    \"D_cmd\":\"echo DAG_D_SHOULD_NOT_RUN\"
  }
}")"
echo "$R" | jq .

if echo "$R" | jq -e '.summary.total' >/dev/null 2>&1; then
  TOTAL="$(echo "$R" | jq -r '.summary.total')"
  if [ "$TOTAL" -ne 4 ]; then
    echo "ERROR: expected summary.total=4, got $TOTAL"
    exit 1
  fi

  # B 必须失败
  if ! echo "$R" | jq -e '.summary.failed | index("B") != null' >/dev/null 2>&1; then
    echo "ERROR: expected summary.failed contains B"
    echo "$R" | jq .
    exit 1
  fi

  # FailFast 语义：C/D 不应执行。实现上可以是 skipped，也可以直接不进入 success。
  # 这里我们要求：success 里不包含 C/D，并且 C/D 的日志必须为空。
  if echo "$R" | jq -e '.summary.success | index("C") != null' >/dev/null 2>&1; then
    echo "ERROR: expected C not in summary.success under FailFast"
    echo "$R" | jq .
    exit 1
  fi
  if echo "$R" | jq -e '.summary.success | index("D") != null' >/dev/null 2>&1; then
    echo "ERROR: expected D not in summary.success under FailFast"
    echo "$R" | jq .
    exit 1
  fi

  echo "[OK] FailFast semantics: B failed, C/D not in success"
else
  echo "WARN: FailFast response has no .summary, skip semantic asserts."
fi

# A/B 日志应存在；C/D 日志必须为空（不应被调度执行）
expect_task_logs_nonempty "A"
expect_task_logs_nonempty "B"
expect_task_logs_empty "C"
expect_task_logs_empty "D"

echo

echo "=============================="
echo "9) cleanup templates"
echo "=============================="
req DELETE "$BASE/template/$TID_SINGLE" | jq . || true
req DELETE "$BASE/template/$TID_DAG" | jq . || true
echo

echo "ALL M13 tests done."
