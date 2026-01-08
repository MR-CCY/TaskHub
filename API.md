# TaskHub Server API and WS Protocol

This doc summarizes HTTP endpoints and WebSocket protocol as implemented under `server/src/handlers` and `server/src/ws`.

## Conventions
- Base URL: `http://<host>:<port>`
- Auth: protected APIs expect `Authorization: Bearer <token>` (from `/api/login`).
- Content-Type: JSON bodies use `application/json`.
- Response envelope (most endpoints):
  - Success: `{"code":0,"message":"ok","data":...}`
  - Error: `{"code":<non-zero>,"message":"<error>","data":null}`
- HTTP status: some handlers return 4xx/5xx; others return 200 with `code != 0`. Always check `code`.
- Some endpoints (template/cron/worker) place `ok`/`error` inside `data` for success; errors may return a JSON string in `message`.

## Common Schemas

### TaskStatus (string)
- `pending` | `running` | `success` | `failed` | `canceled` | `timeout`

### TaskExecType (string)
- `Local` | `Remote` | `Script` | `HttpCall` | `Shell`

### TaskPriority (int)
- `-1` Low, `0` Normal, `1` High, `2` Critical
- Note: server may coerce Critical to High.

### TaskConfig (request)
Supported shapes:
- `{ "task": { ... } }`
- `{ ... }` (direct task object)

Fields:
- `id` (string, required for DAG and worker execute)
- `name` (string, optional)
- `exec_type` (string, default `Local`)
- `exec_command` (string)
- `exec_params` (object, optional; values are strings)
- `timeout_ms` (int, default 0)
- `retry_count` (int, default 0)
- `retry_delay_ms` (int, default 1000)
- `retry_exp_backoff` (bool, default true)
- `priority` (int, default 0)
- `queue` (string, optional)
- `capture_output` (bool, default true)
- `metadata` (object, optional; values are strings)

### TaskResult (response)
- `status` (int enum)
- `message` (string)
- `exit_code` (int)
- `duration_ms` (int)
- `stdout` (string)
- `stderr` (string)
- `attempt` (int)
- `max_attempts` (int)
- `metadata` (object)

### DagConfig
- `name` (string, optional)
- `fail_policy` (string: `SkipDownstream` or `FailFast`)
- `max_parallel` (int)

### WorkerInfo
- `id` (string)
- `host` (string)
- `port` (int)
- `queues` (array of string)
- `labels` (array of string)
- `running_tasks` (int)
- `max_running_tasks` (int)
- `full` (bool)
- `alive` (bool)
- `last_seen_ms_ago` (int)

## HTTP APIs

### Auth

#### POST /api/login
Request:
- Headers: `Content-Type: application/json`
- Body:
```json
{
  "username": "admin",
  "password": "123456"
}
```

Response (success):
```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "token": "<jwt-like>",
    "username": "admin"
  }
}
```

Response (errors):
```json
{ "code": 1001, "message": "Content-Type must be application/json", "data": null }
{ "code": 1002, "message": "Invalid JSON body", "data": null }
{ "code": 1003, "message": "username and password required", "data": null }
{ "code": 1004, "message": "invalid_username_or_password", "data": null }
```

### System

#### GET /api/health
Response:
```json
{
  "code": 0,
  "message": "ok",
  "data": { "status": "healthy", "time": "2025-01-01 12:00:00", "uptime_sec": 1 }
}
```

#### GET /api/info
Response:
```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "name": "TaskHub Server",
    "version": "0.1.0",
    "build_time": "Jan 01 2025 12:00:00",
    "commit": "dev-local"
  }
}
```

#### GET /api/stats
Response:
```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "worker_pool": { "workers": 1, "queued": 0, "running": 0, "submitted": 0, "finished": 0 },
    "ws": { "sessions": 0 }
  }
}
```

#### GET /api/debug/broadcast
Side effect: broadcasts WS message `{"event":"debug","data":{"msg":"hello from TaskHub WS"}}`.
Response:
```json
{ "code": 0, "message": "ok", "data": null }
```

### Tasks (Auth required)

#### POST /api/tasks
Request body:
```json
{
  "name": "task1",
  "type": 0,
  "params": { "cmd": "echo hi" }
}
```

Response (success):
```json
{ "code": 0, "message": "ok", "data": { "id": 123 } }
```

Response (errors):
```json
{ "code": 1001, "message": "Missing 'name' field", "data": null }
{ "code": 1002, "message": "Invalid JSON", "data": null }
{ "code": 401, "message": "unauthorized", "data": null }
```

#### GET /api/tasks
Response (example item fields):
```json
{
  "code": 0,
  "message": "ok",
  "data": [
    {
      "id": 123,
      "name": "task1",
      "type": 0,
      "status": "pending",
      "create_time": "2025-01-01 12:00:00",
      "update_time": "2025-01-01 12:00:01",
      "params": { "cmd": "echo hi" },
      "exit_code": 0,
      "last_output": "",
      "last_error": "",
      "max_retries": 0,
      "retry_count": 0,
      "timeout_sec": 0,
      "cancel_flag": false
    }
  ]
}
```

#### GET /api/tasks/{id}
Response (success):
```json
{ "code": 0, "message": "ok", "data": { "...same fields as list item..." } }
```

Response (errors):
```json
{ "code": 404, "message": "Task not found", "data": null }
{ "code": 401, "message": "unauthorized", "data": null }
```

#### POST /api/tasks/{id}/cancel
Response (success):
```json
{ "code": 0, "message": "ok", "data": null }
```

Response (errors):
```json
{ "code": 1003, "message": "Task not found", "data": null }
{ "code": 1004, "message": "Task already finished", "data": null }
{ "code": 401, "message": "unauthorized", "data": null }
```

### DAG

#### POST /api/dag/run
Request body:
```json
{
  "name": "workflow-1",
  "config": { "name": "workflow-1", "fail_policy": "SkipDownstream", "max_parallel": 4 },
  "tasks": [
    {
      "id": "a",
      "name": "step-a",
      "exec_type": "Shell",
      "exec_command": "echo hi",
      "deps": []
    }
  ]
}
```

Response (success):
```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "ok": true,
    "message": "ok",
    "nodes": [
      { "id": "a", "run_id": "1767600000000_ABCDEF", "result": { "status": 2, "message": "ok" } }
    ],
    "run_id": "1767600000000_ABCDEF",
    "summary": { "total": 1, "success": ["a"], "failed": [], "skipped": [] }
  }
}
```

Response (errors):
```json
{ "code": 500, "message": "{\"ok\":false,\"error\":\"<dag error>\"}", "data": null }
```

#### POST /api/dag/run_async
Request body: same shape as `/api/dag/run` (must include `tasks` array).

Response (success):
```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "run_id": "1767600000000_ABCDEF",
    "task_ids": [
      { "logical": "a", "task_id": "a" }
    ]
  }
}
```

Response (errors):
```json
{ "code": 1002, "message": "Invalid JSON", "data": null }
{ "code": 400, "message": "{\"error\":\"tasks must be array\"}", "data": null }
```

#### GET /api/dag/runs
Query params:
- `run_id` (string, exact match)
- `name` (string, fuzzy match)
- `start_ts_ms` (int)
- `end_ts_ms` (int)
- `limit` (int, default 100, max 500)

Response:
```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "items": [
      {
        "run_id": "1767600000000_ABCDEF",
        "name": "workflow-1",
        "source": "manual",
        "status": "success",
        "start_ts_ms": 1767600000000,
        "end_ts_ms": 1767600000100,
        "total": 1,
        "success_count": 1,
        "failed_count": 0,
        "skipped_count": 0,
        "message": "ok"
      }
    ]
  }
}
```

#### GET /api/dag/task_runs
Query params:
- `run_id` (string, exact match)
- `name` (string, exact match)
- `limit` (int, default 200, max 1000)

Response:
```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "items": [
      {
        "id": 1,
        "run_id": "1767600000000_ABCDEF",
        "logical_id": "a",
        "task_id": "a",
        "name": "step-a",
        "exec_type": "Shell",
        "exec_command": "echo hi",
        "exec_params": {},
        "deps": [],
        "status": "success",
        "exit_code": 0,
        "duration_ms": 12,
        "message": "",
        "stdout": "hi\n",
        "stderr": "",
        "attempt": 1,
        "max_attempts": 1,
        "start_ts_ms": 1767600000000,
        "end_ts_ms": 1767600000012
      }
    ]
  }
}
```

#### GET /api/dag/events
Query params:
- `run_id` (string)
- `task_id` (string)
- `type` (string)
- `event` (string)
- `start_ts_ms` (int)
- `end_ts_ms` (int)
- `limit` (int, default 200, max 1000)

Response:
```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "items": [
      {
        "id": 1,
        "task_id": "a",
        "run_id": "1767600000000_ABCDEF",
        "type": "event",
        "event": "task_start",
        "ts_ms": 1767600000000,
        "extra": {}
      }
    ]
  }
}
```

### Templates

#### POST /template
Request body:
```json
{
  "template_id": "t1",
  "name": "echo template",
  "description": "sample",
  "task_json_template": { "...": "..." },
  "schema": { "...": "..." }
}
```

Response (success):
```json
{ "code": 0, "message": "ok", "data": { "ok": true, "template_id": "t1" } }
```

Response (errors):
```json
{ "code": 400, "message": "{\"ok\":false,\"error\":\"missing required fields\"}", "data": null }
{ "code": 500, "message": "{\"ok\":false,\"error\":\"template id already exists\"}", "data": null }
```

#### GET /template/{id}
Response (success):
```json
{ "code": 0, "message": "ok", "data": { "ok": true, "data": { "...template..." } } }
```

Response (errors):
```json
{ "code": 404, "message": "{\"ok\":false,\"error\":\"template not found\"}", "data": null }
```

#### GET /templates
Response:
```json
{ "code": 0, "message": "ok", "data": { "ok": true, "data": [ { "...template..." } ] } }
```

#### DELETE /template/{id}
Response:
```json
{ "code": 0, "message": "ok", "data": { "ok": true } }
```

#### POST /template/update/{id}
Note: not implemented.

#### POST /template/render
Request body:
```json
{ "template_id": "t1", "params": { "key": "value" } }
```

Response:
```json
{
  "code": 0,
  "message": "ok",
  "data": { "ok": true, "data": { "ok": true, "error": "", "rendered": { "...": "..." } } }
}
```

#### POST /template/run
Request body: same as `/template/render`.

Response (success):
```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "ok": true,
    "message": "ok",
    "nodes": [ { "id": "a", "run_id": "...", "result": { "status": 2 } } ],
    "run_id": "1767600000000_ABCDEF",
    "summary": { "total": 1, "success": ["a"], "failed": [], "skipped": [] }
  }
}
```

#### POST /template/run_async
Response:
```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "run_id": "1767600000000_ABCDEF",
    "task_ids": [ { "logical": "a", "task_id": "a" } ]
  }
}
```

### Logs

#### GET /api/tasks/logs
Query params:
- `task_id` (string, required)
- `run_id` (string, optional)
- `from` (int, default 1)
- `limit` (int, default 200, max 2000)

Response:
```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "task_id": "task-1",
    "from": 1,
    "limit": 200,
    "next_from": 201,
    "records": [
      {
        "seq": 1,
        "task_id": "task-1",
        "run_id": "1767600000000_ABCDEF",
        "level": 2,
        "stream": 1,
        "message": "hello",
        "ts_ms": 1767600000000,
        "duration_ms": 10,
        "attempt": 1,
        "fields": {}
      }
    ]
  }
}
```

### Cron

#### GET /api/cron/jobs
Response:
```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "ok": true,
    "jobs": [
      {
        "id": "job_1767600000000",
        "name": "job1",
        "spec": "*/1 * * * *",
        "enabled": true,
        "target_type": "SingleTask",
        "next_time_epoch": 1767600060,
        "summary": { "exec_type": "Shell", "exec_command": "echo hi" }
      }
    ]
  }
}
```

#### POST /api/cron/jobs
SingleTask body:
```json
{
  "name": "job1",
  "spec": "*/1 * * * *",
  "target_type": "SingleTask",
  "task": {
    "id": "task",
    "name": "echo",
    "exec_type": "Shell",
    "exec_command": "echo hi",
    "timeout_ms": 0,
    "retry_count": 0,
    "priority": 0
  }
}
```

Dag body:
```json
{
  "name": "job1",
  "spec": "*/1 * * * *",
  "target_type": "Dag",
  "dag": {
    "config": { "fail_policy": "SkipDownstream", "max_parallel": 4 },
    "tasks": [ { "id": "a", "name": "a", "exec_type": "Shell", "exec_command": "echo hi" } ]
  }
}
```

Template body:
```json
{
  "name": "job1",
  "spec": "*/1 * * * *",
  "target_type": "Template",
  "template": {
    "template_id": "tpl-1",
    "params": { "name": "demo" }
  }
}
```

Response:
```json
{ "code": 0, "message": "ok", "data": { "ok": true, "job_id": "job_1767600000000" } }
```

#### DELETE /api/cron/jobs/{id}
Response:
```json
{ "code": 0, "message": "ok", "data": { "ok": true } }
```

### Workers

#### POST /api/workers/register
Request body:
```json
{
  "id": "worker-1",
  "host": "127.0.0.1",
  "port": 8083,
  "queues": ["default"],
  "labels": ["shell"],
  "running_tasks": 0,
  "max_running_tasks": 2
}
```

Response:
```json
{ "code": 0, "message": "ok", "data": { "ok": true } }
```

#### POST /api/workers/heartbeat
Request body:
```json
{ "id": "worker-1", "running_tasks": 2 }
```

Response:
```json
{ "code": 0, "message": "ok", "data": { "ok": true } }
```

Errors:
```json
{ "code": 400, "message": "missing worker id", "data": null }
{ "code": 404, "message": "worker not found", "data": null }
```

#### GET /api/workers
Response:
```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "ok": true,
    "workers": [
      {
        "id": "worker-1",
        "host": "127.0.0.1",
        "port": 8083,
        "queues": ["default"],
        "labels": ["shell"],
        "running_tasks": 1,
        "max_running_tasks": 2,
        "full": false,
        "alive": true,
        "last_seen_ms_ago": 1200
      }
    ]
  }
}
```

#### GET /api/workers/connected
Same payload shape as `/api/workers`, but only includes `alive=true` workers.

#### GET /api/workers/proxy/logs
转发到指定 Worker 的日志查询（等同 `/api/tasks/logs`）。

Query:
- `task_id` (required)
- `run_id` (required when using `remote_path`)
- `remote_path` (optional, multi-hop 路径，例如 `R_1/R_2`；为空表示本机查询)
- `worker_id` (optional, legacy: 直连转发到指定 worker；未提供 `remote_path` 时生效)
- `from` (optional, default 1)
- `limit` (optional, default 200)

Response: 同 `/api/tasks/logs`。

Errors:
```json
{ "code": 400, "message": "missing task_id", "data": null }
{ "code": 404, "message": "remote logical node not found: ...", "data": null }
{ "code": 404, "message": "worker_id not found for node: ...", "data": null }
{ "code": 404, "message": "worker not found or offline: ...", "data": null }
{ "code": 404, "message": "worker not alive: ...", "data": null }
{ "code": 502, "message": "proxy forward failed: no response", "data": null }
```

#### GET /api/workers/proxy/dag/task_runs
按 `remote_path` 逐跳转发 `/api/dag/task_runs`（用于 Remote 嵌套 DAG 的跨 worker 查询）。

Query:
- `run_id` (required when using `remote_path`)
- `remote_path` (optional, multi-hop 路径；为空表示本机查询)
- `name` (optional)
- `limit` (optional, default 200)

Response: 同 `/api/dag/task_runs`。

Errors:
```json
{ "code": 404, "message": "remote logical node not found: ...", "data": null }
{ "code": 404, "message": "worker_id not found for node: ...", "data": null }
{ "code": 404, "message": "worker not found or offline: ...", "data": null }
{ "code": 404, "message": "worker not alive: ...", "data": null }
{ "code": 502, "message": "proxy forward failed: no response", "data": null }
```

#### GET /api/workers/proxy/dag/events
按 `remote_path` 逐跳转发 `/api/dag/events`（用于 Remote 嵌套 DAG 的跨 worker 查询）。

Query:
- `run_id` (required when using `remote_path`)
- `remote_path` (optional, multi-hop 路径；为空表示本机查询)
- `task_id` (optional)
- `type` (optional)
- `event` (optional)
- `start_ts_ms` (optional)
- `end_ts_ms` (optional)
- `limit` (optional, default 200)

Response: 同 `/api/dag/events`。

Errors:
```json
{ "code": 404, "message": "remote logical node not found: ...", "data": null }
{ "code": 404, "message": "worker_id not found for node: ...", "data": null }
{ "code": 404, "message": "worker not found or offline: ...", "data": null }
{ "code": 404, "message": "worker not alive: ...", "data": null }
{ "code": 502, "message": "proxy forward failed: no response", "data": null }
```

#### POST /api/worker/execute
Request body（当前用于 Remote 节点的 DAG 异步派发）：
```json
{
  "type": "dag",
  "payload": {
    "name": "wf-1",
    "config": { "fail_policy": "SkipDownstream", "max_parallel": 4 },
    "tasks": [
      { "id": "a", "exec_type": "Shell", "exec_command": "echo a" },
      { "id": "b", "exec_type": "Shell", "exec_command": "echo b", "deps": ["a"] }
    ]
  }
}
```
Response (dispatch ack):
```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "type": "dag",
    "ok": true,
    "run_id": "1767600000000_abcd12",
    "task_ids": [
      { "logical": "a", "task_id": "a" },
      { "logical": "b", "task_id": "b" }
    ]
  }
}
```

Errors:
```json
{ "code": 400, "message": "invalid json", "data": null }
{ "code": 400, "message": "worker only supports dag payload for Remote nodes", "data": null }
```

Remote 任务（Master -> Worker）：
- `exec_type: "Remote"` 需要提供 `exec_params.dag_json`（DAG JSON 字符串）
- Master 会读取 `dag_json` 并调用 Worker 的 `/api/worker/execute`（见上面的 `type=dag` 请求示例）

本地同步 DAG / Template 任务（exec_type）：
- `exec_type: "Dag"`，`exec_params.dag_json` = DAG JSON 字符串
- `exec_type: "Template"`，`exec_params.template_id` + `exec_params.template_params_json`（JSON 字符串）
- 会生成新的 `run_id` 并落库（`dag_run`/`task_run`）；payload 内部的 `run_id` 会被覆盖

示例（Dag 任务）：
```json
{
  "id": "dag-task-1",
  "exec_type": "Dag",
  "exec_params": {
    "dag_json": "{\"tasks\":[{\"id\":\"a\",\"exec_type\":\"Shell\",\"exec_command\":\"echo a\"}]}"
  }
}
```

示例（Template 任务）：
```json
{
  "id": "tpl-task-1",
  "exec_type": "Template",
  "exec_params": {
    "template_id": "tpl-1",
    "template_params_json": "{\"name\":\"demo\"}"
  }
}
```

## WebSocket Protocol (Beast server)

### Connect and Auth
1) Connect to WS server.
2) First message must contain `token`:
```json
{ "token": "<jwt>" }
```
Server replies:
```json
{ "type": "authed" }
```

### Commands
- Subscribe:
```json
{ "token": "<jwt>", "op": "subscribe", "topic": "task_logs|task_events", "task_id": "t1", "run_id": "r1" }
```
- Unsubscribe:
```json
{ "token": "<jwt>", "op": "unsubscribe", "topic": "task_logs|task_events", "task_id": "t1", "run_id": "r1" }
```
- Ping:
```json
{ "token": "<jwt>", "op": "ping" }
```
Server replies:
```json
{ "type": "pong" }
```

- Proxy to worker WebSocket:
```json
{ "token": "<jwt>", "op": "proxy", "worker_id": "worker-1", "ws_port": 8090 }
```
Server replies:
```json
{ "type": "proxy_ready", "worker_id": "worker-1", "ws_port": 8090 }
```
随后客户端发出的消息会原样转发到 Worker 的 WS，Worker 的推送也会透传返回。

Rate limit: 30 commands per 10 seconds window.

### Channels
- `task_logs` -> `task.logs.<task_id>` or `task.logs.<task_id>.<run_id>`
- `task_events` -> `task.events.<task_id>` or `task.events.<task_id>.<run_id>`

### Server push (subscribed only)
- Log:
```json
{
  "type": "log",
  "task_id": "t1",
  "run_id": "r1",
  "seq": 1,
  "ts_ms": 1767600000000,
  "level": 2,
  "stream": 1,
  "message": "log line",
  "duration_ms": 10,
  "attempt": 1,
  "fields": {}
}
```

- Event:
```json
{
  "type": "event",
  "task_id": "t1",
  "run_id": "r1",
  "event": "task_start",
  "ts_ms": 1767600000000,
  "extra": {}
}
```

### Global broadcast
Example debug broadcast:
```json
{ "event": "debug", "data": { "msg": "hello from TaskHub WS" } }
```
