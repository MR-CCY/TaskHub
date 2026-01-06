#!/usr/bin/env bash
set -euo pipefail

BASES_RAW="${BASES:-}"
if [[ -n "${BASES_RAW}" ]]; then
  BASES_RAW="${BASES_RAW//,/ }"
  read -r -a BASES <<< "${BASES_RAW}"
else
  BASES=("http://localhost:8082" "http://localhost:8083" "http://localhost:8084")
fi
USER_NAME="${TASKHUB_USER:-admin}"
USER_PASS="${TASKHUB_PASS:-123456}"
TEMPLATE_SUFFIX="${TEMPLATE_SUFFIX:-}"

need_json_parser() {
  if command -v jq >/dev/null 2>&1; then
    echo "jq"
    return 0
  fi
  if command -v python3 >/dev/null 2>&1; then
    echo "python3"
    return 0
  fi
  echo "none"
  return 1
}

JSON_TOOL="$(need_json_parser)"
if [[ "${JSON_TOOL}" == "none" ]]; then
  echo "jq or python3 is required to parse login response." >&2
  exit 1
fi

get_token() {
  local base="$1"
  local resp
  resp="$(curl -sS -X POST "${base}/api/login" \
    -H "Content-Type: application/json" \
    -d "{\"username\":\"${USER_NAME}\",\"password\":\"${USER_PASS}\"}")"

  if [[ "${JSON_TOOL}" == "jq" ]]; then
    echo "${resp}" | jq -r '.data.token // empty'
  else
    python3 - <<'PY' <<<"${resp}"
import json,sys
try:
    data=json.load(sys.stdin)
    print(data.get("data",{}).get("token",""))
except Exception:
    print("")
PY
  fi
}

post_template() {
  local base="$1"
  local token="$2"
  local name="$3"
  local payload="$4"

  local resp
  resp="$(curl -sS -X POST "${base}/template" \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer ${token}" \
    -d "${payload}")"

  if echo "${resp}" | grep -q "template id already exists"; then
    echo "  - ${name}: exists, skip"
    return 0
  fi

  echo "  - ${name}: ${resp}"
}

build_tpl_echo() {
  cat <<JSON
{
  "template_id": "tpl_echo${TEMPLATE_SUFFIX}",
  "name": "echo template",
  "description": "echo a message",
  "task_json_template": {
    "task": {
      "id": "{{task_id}}",
      "name": "echo-{{task_id}}",
      "exec_type": "Shell",
      "exec_command": "echo {{msg}}",
      "queue": "{{queue}}"
    }
  },
  "schema": {
    "params": [
      { "name": "task_id", "type": "string", "required": true },
      { "name": "msg", "type": "string", "required": true },
      { "name": "queue", "type": "string", "required": false, "defaultValue": "default" }
    ]
  }
}
JSON
}

build_tpl_sleep() {
  cat <<JSON
{
  "template_id": "tpl_sleep${TEMPLATE_SUFFIX}",
  "name": "sleep template",
  "description": "sleep for N seconds",
  "task_json_template": {
    "task": {
      "id": "{{task_id}}",
      "name": "sleep-{{seconds}}",
      "exec_type": "Shell",
      "exec_command": "sleep {{seconds}}"
    }
  },
  "schema": {
    "params": [
      { "name": "task_id", "type": "string", "required": true },
      { "name": "seconds", "type": "int", "required": false, "defaultValue": 1 }
    ]
  }
}
JSON
}

build_tpl_http() {
  cat <<JSON
{
  "template_id": "tpl_http${TEMPLATE_SUFFIX}",
  "name": "http template",
  "description": "http call with method and url",
  "task_json_template": {
    "task": {
      "id": "{{task_id}}",
      "name": "http-{{task_id}}",
      "exec_type": "HttpCall",
      "exec_command": "{{url}}",
      "exec_params": {
        "method": "{{method}}",
        "header.Content-Type": "{{content_type}}"
      }
    }
  },
  "schema": {
    "params": [
      { "name": "task_id", "type": "string", "required": true },
      { "name": "url", "type": "string", "required": true },
      { "name": "method", "type": "string", "required": false, "defaultValue": "GET" },
      { "name": "content_type", "type": "string", "required": false, "defaultValue": "application/json" }
    ]
  }
}
JSON
}

for base in "${BASES[@]}"; do
  echo "Seeding templates on ${base}"
  token="$(get_token "${base}")"
  if [[ -z "${token}" ]]; then
    echo "  - login failed for ${base}" >&2
    continue
  fi

  post_template "${base}" "${token}" "tpl_echo${TEMPLATE_SUFFIX}" "$(build_tpl_echo)"
  post_template "${base}" "${token}" "tpl_sleep${TEMPLATE_SUFFIX}" "$(build_tpl_sleep)"
  post_template "${base}" "${token}" "tpl_http${TEMPLATE_SUFFIX}" "$(build_tpl_http)"
done
