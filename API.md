# TaskHub Server API & WS Protocol

This doc summarizes the HTTP endpoints and WebSocket protocol as implemented under `server/src/handlers` and `server/src/ws`.

## Conventions
- Auth: protected APIs expect `Authorization: Bearer <token>`. Token is obtained via `/api/login`.
- Content-Type: JSON bodies should use `application/json`.
- Response envelopes:
  - Most `/api/**` use `{"code":<int>,"message":<string>,"data":...}`.
  - Template/cron/worker endpoints use `{"ok":true/false,...}`.
  - Errors may return HTTP 4xx/5xx plus the envelope above.

## HTTP APIs

### Auth
- `POST /api/login`
  - Body: `{"username":"admin","password":"123456"}`
  - 200: `{"code":0,"message":"ok","data":{"token":"<jwt-like>","username":"admin"}}`
  - Errors: `code=1001` content-type not json; `1002` invalid json; `1003` missing fields; `1004` invalid credentials.

### System
- `GET /api/health`
  - 200: `{"code":0,"message":"ok","data":{"status":"healthy","time":"<ISO>","uptime_sec":<int>}}`
- `GET /api/info`
  - 200: `{"code":0,"message":"ok","data":{"name":"TaskHub Server","version":"0.1.0","build_time":"<date time>","commit":"dev-local"}}`
- `GET /api/stats`
  - 200: `{"code":0,"message":"ok","data":{"worker_pool":{"workers":int,"queued":int,"running":int,"submitted":int,"finished":int},"ws":{"sessions":int}}}`
- `GET /api/debug/broadcast`
  - 200: `{"code":0,"message":"ok","data":null}`; side effect: WS broadcast `{"event":"debug","data":{"msg":"hello from TaskHub WS"}}`.

### Tasks (Auth required)
- `POST /api/tasks`
  - Body: `{"name":"task1","type":0,"params":{...}}`
  - 200: `{"code":0,"message":"ok","data":{"id":<int>}}`
  - Errors: `1001` missing name; `1002` invalid json; `401` unauthorized.
- `GET /api/tasks`
  - 200: `{"code":0,"message":"ok","data":[{id,name,type,status,create_time,update_time,params,exit_code,last_output,last_error,max_retries,retry_count,timeout_sec,cancel_flag}]}`
  - 401 if unauthorized.
- `GET /api/tasks/{id}`
  - 200: `{"code":0,"message":"ok","data":{...same fields as list...}}`
  - 404 `{"code":404,"message":"Task not found"}`; 401 unauthorized.
- `POST /api/tasks/{id}/cancel`
  - 200: `{"code":0,"message":"ok","data":null}`
  - Errors: `1003` task not found; `1004` already finished; 401 unauthorized.

### DAG run
- `POST /api/dag/run`
  - Body: `{"name":"workflow-1","config":{"name":"workflow-1","fail_policy":"SkipDownstream"|"FailFast","max_parallel":4},"tasks":[{"id":"a","name":"step-a","exec_type":"Local",...,"deps":["b"]},...]}` (or single `task` object)
  - 200: `{"ok":true,"message":string,"nodes":[{"id":"a","run_id":"...","result":{status:int,message,exit_code,duration_ms,stdout,stderr,attempt,max_attempts,metadata}}],"run_id":string,"summary":{"total":N,"success":[ids],"failed":[ids],"skipped":[ids]}}`
  - 500: `{"ok":false,"error":<dag error>}`
- `POST /api/dag/run_async`
  - Body same as `/api/dag/run` (must include `tasks` array, optional top-level/config `name`).
  - 200: `{"code":0,"message":"ok","data":{"run_id":string,"task_ids":[{"logical":<id>,"task_id":<id>},...]}}`
  - Errors: `1002` invalid json; `400` `{"error":"tasks must be array"}`; other via resp::error.
- `GET /api/dag/runs?run_id=<id>&name=<fuzzy>&start_ts_ms=<from>&end_ts_ms=<to>&limit=<n>`
  - Filters: `run_id` exact, `name` fuzzy match, start/end timestamps (ms). `limit` default 100, max 500.
  - 200: `{"code":0,"message":"ok","data":{"items":[{run_id,name,source,status,start_ts_ms,end_ts_ms,total,success_count,failed_count,skipped_count,message}]}}`
- `GET /api/dag/task_runs?run_id=<id>&name=<exact>&limit=<n>`
  - Filters: `run_id` exact, `name` exact (non-fuzzy). `limit` default 200, max 1000.
  - 200: `{"code":0,"message":"ok","data":{"items":[{id,run_id,logical_id,task_id,name,exec_type,exec_command,exec_params,deps,status,exit_code,duration_ms,message,stdout,stderr,attempt,max_attempts,start_ts_ms,end_ts_ms}]}}`
- `GET /api/dag/events?run_id=<id>&task_id=<id>&type=<type>&event=<event>&start_ts_ms=<from>&end_ts_ms=<to>&limit=<n>`
  - Filters: any combination of run_id/task_id/type/event and time range; `limit` default 200, max 1000.
  - 200: `{"code":0,"message":"ok","data":{"items":[{id,task_id,run_id,type,event,ts_ms,extra...,raw payload fields}]}}`

### Templates
- `POST /template`
  - Body: `{"template_id":"t1","name":"...","description":"...","task_json_template":{...},"schema":{...}}`
  - 200: `{"ok":true,"template_id":"t1"}`
  - Errors: 400 bad request / missing fields; 500 duplicate id or internal.
- `GET /template/{id}`
  - 200: `{"ok":true,"data":{template...}}`
  - 404 template not found.
- `GET /templates`
  - 200: `{"ok":true,"data":[{template...}]}`; 500 internal.
- `DELETE /template/{id}`
  - 200: `{"ok":true}`
  - 404 template not found; 500 internal.
- `POST /template/update/{id}`
  - TODO (not implemented).
- `POST /template/render`
  - Body: `{"template_id":"t1","params":{...}}`
  - 200: `{"ok":true,"data":{"ok":true,"error":"","rendered":{...}}}`
  - Errors: 400 missing fields or render error (`data` contains RenderResult); 404 template not found; 500 internal.
- `POST /template/run`
  - Body: `{"template_id":"t1","params":{...}}`; injects `run_id` into rendered tasks then runs DAG.
  - 200: `{"ok":true,"message":string,"nodes":[{"id":"a","run_id":"...","result":{...}}],"run_id":string,"summary":{"total":N,"success":[ids],"failed":[ids],"skipped":[ids]}}`
  - Errors: 400 missing fields/render error; 404 template not found; 500 dag/internal error.
- `POST /template/run_async`
  - Body: `{"template_id":"t1","params":{...}}`; injects `run_id`, executes DAG in background.
  - 200: `{"code":0,"message":"ok","data":{"run_id":string,"task_ids":[{"logical":<task logical id>,"task_id":<task id>},...]}}`
  - Errors: 400 bad request/render error; 404 template not found; 500 internal.

### Logs
- `GET /api/tasks/logs?task_id=<id>&run_id=<rid?>&from=<seq?>&limit=<n?>`
  - `from` default 1; `limit` default 200, clamped 1..2000.
  - 200: `{"code":0,"message":"ok","data":{"task_id":string,"from":u64,"limit":int,"next_from":u64,"records":[{seq,task_id,run_id?,dag_run_id?,cron_job_id?,worker_id?,level:int,stream:int,message,ts_ms,duration_ms,attempt,fields:{}}]}}`
  - 400 missing task_id; 500 query failure.

### Cron
- `GET /api/cron/jobs`
  - 200: `{"ok":true,"jobs":[{id,name,spec,enabled,target_type,next_time_epoch,summary:{exec_type,exec_command?}}]}`
- `POST /api/cron/jobs`
  - Body:
    - SingleTask: `{"name":"job1","spec":"*/1 * * * *","target_type":"SingleTask","task":{"id":"task","name":"...","exec_type":"Local|Remote|Shell","exec_command":"...","timeout_ms":0,"retry_count":0,"priority":"Normal|Low|High|Critical"}}`
    - Dag: `{"name":"job1","spec":"*/1 * * * *","target_type":"Dag","dag":{"config":{"fail_policy":"SkipDownstream"|"FailFast","max_parallel":4},"tasks":[{"id":"a","deps":["b"],"name":"a","timeout_ms":0,"retry_count":0,"retryDelay":1000,"exec_type":"Local","exec_command":"..."}]}}`
  - 200: `{"ok":true,"job_id":"job_x"}`
  - 400: `{"ok":false,"message":"..."}` validation/parse errors.
- `DELETE /api/cron/jobs/{id}`
  - 200: `{"ok":true}`; 400 missing id.

### Workers
- `POST /api/workers/register`
  - Body: `{"id":"worker-1","host":"127.0.0.1","port":8083,"queues":["default"],"labels":["gpu"],"running_tasks":0}`
  - 200: `{"ok":true}`
  - Errors: 400 invalid json or missing id/port; 500 internal.
- `POST /api/workers/heartbeat`
  - Body: `{"id":"worker-1","running_tasks":2}`
  - 200: `{"ok":true}`
  - Errors: 400 missing id; 404 worker not found; 500 internal.
- `GET /api/workers`
  - 200: `{"ok":true,"workers":[{id,host,port,running_tasks,alive,last_seen_ms_ago,queues[],labels[]}]}`
  - 500 internal.
- `POST /api/worker/execute`
  - Body: TaskConfig JSON (same shape as DAG tasks); `exec_type` must not be `Remote`.
  - 200: `{"status":int,"message":string,"exit_code":int,"duration_ms":<ms>,"stdout":string,"stderr":string,"attempt":int,"max_attempts":int,"metadata":{...}}`
  - Errors: 400 invalid json or exec_type Remote; 500 internal.

## WebSocket Protocol (Beast server)
Flow:
1) Connect, then first message **must** contain `token`.
   - Minimal auth-only: `{"token":"<jwt>"}` → server replies `{"type":"authed"}`.
   - Or include command fields below in the first message.
2) Commands:
   - Subscribe: `{"token":"<jwt>","op":"subscribe","topic":"task_logs|task_events","task_id":"t1","run_id":"r1?"}`
   - Unsubscribe: same shape with `op:"unsubscribe"`.
   - Ping: `{"token":"<jwt>","op":"ping"}` → server replies `{"type":"pong"}`.
   - Rate limiting: 30 commands / 10s window; exceeded → connection closed.
3) Channels:
   - task_logs → `task.logs.<task_id>` or `task.logs.<task_id>.<run_id>`
   - task_events → `task.events.<task_id>` or `task.events.<task_id>.<run_id>`
4) Server push (only to subscribed channels):
   - Log: `{"type":"log","task_id":"t1","run_id":"r1?","seq":1,"ts_ms":<ms>,"level":int,"stream":int,"message":"...","duration_ms":<ms>,"attempt":1,"fields":{...}}`
   - Event: `{"type":"event","task_id":"t1","run_id":"r1?","event":"task_start","ts_ms":<ms>,"extra":{...}}`
5) Global broadcast (no subscription needed): e.g. `broadcast_task_event` sends `{"event":"task_updated","data":{task...}}`; debug endpoint sends `{"event":"debug","data":{"msg":"hello from TaskHub WS"}}`.
