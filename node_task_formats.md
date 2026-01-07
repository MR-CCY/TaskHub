# 节点任务格式（Remote / DAG / Template）

下面的示例基于 `remote_strategy.cpp`、`dag_strategy.cpp`、`template_strategy.cpp` 的实际解析逻辑整理。

## 通用说明
- 任务结构按 `TaskConfig`（`id/name/exec_type/exec_command/exec_params/...`）。
- 在客户端导出时，`exec_params` 的值会被序列化为 **字符串**，因此：
  - `dag_json`、`template_params_json`、`remote.payload_json` 都应是 **JSON 字符串**。
- `exec_type` 支持大小写，服务端会做 `toLower()` 处理。

## Remote 节点（exec_type = "Remote"）

### 1) 远程执行“单任务”
默认路径是 `inner.exec_*`，最终发送给 worker 的是一个“普通 task”。

```json
{
  "id": "R_1",
  "name": "remote_shell",
  "exec_type": "Remote",
  "exec_command": "",
  "exec_params": {
    "inner.exec_type": "Shell",
    "inner.exec_command": "echo hello",
    "inner.exec_params.cwd": "/tmp",
    "inner.exec_params.shell": "/bin/bash"
  },
  "timeout_ms": 0,
  "retry_count": 0,
  "retry_delay_ms": 1000,
  "retry_exp_backoff": true,
  "queue": "default",
  "capture_output": true
}
```

### 2) 远程执行“DAG”
走 worker 的 `payload_type=dag` 分支，`remote.payload_json` 必须是 **JSON 字符串**。

```json
{
  "id": "R_2",
  "name": "remote_dag",
  "exec_type": "Remote",
  "exec_params": {
    "remote.payload_type": "dag",
    "remote.payload_json": "{\"config\":{\"name\":\"sub_dag\",\"fail_policy\":\"SkipDownstream\",\"max_parallel\":4},\"tasks\":[{\"id\":\"t1\",\"name\":\"A\",\"exec_type\":\"Shell\",\"exec_command\":\"echo A\"},{\"id\":\"t2\",\"name\":\"B\",\"exec_type\":\"Shell\",\"exec_command\":\"echo B\",\"deps\":[\"t1\"]}]}"
  }
}
```

### 3) 远程执行“模板”
走 worker 的 `payload_type=template` 分支，注意 **payload 里是 `template_id + params`**。

```json
{
  "id": "R_3",
  "name": "remote_template",
  "exec_type": "Remote",
  "exec_params": {
    "remote.payload_type": "template",
    "remote.payload_json": "{\"template_id\":\"tpl-001\",\"params\":{\"env\":\"prod\",\"count\":3}}"
  }
}
```

## DAG 节点（exec_type = "Dag"）
服务端读取 `exec_params.dag_json`（为空时会尝试用 `exec_command` 兜底）。

```json
{
  "id": "DAG_1",
  "name": "sub_dag",
  "exec_type": "Dag",
  "exec_params": {
    "dag_json": "{\"config\":{\"name\":\"sub_dag\",\"fail_policy\":\"SkipDownstream\",\"max_parallel\":4},\"tasks\":[{\"id\":\"t1\",\"name\":\"A\",\"exec_type\":\"Shell\",\"exec_command\":\"echo A\"},{\"id\":\"t2\",\"name\":\"B\",\"exec_type\":\"Shell\",\"exec_command\":\"echo B\",\"deps\":[\"t1\"]}]}"
  }
}
```

## 模板节点（exec_type = "Template"）
服务端读取 `exec_params.template_id`，`template_params_json` 可为空字符串（会当作 `{}`）。

```json
{
  "id": "TPL_1",
  "name": "tpl_run",
  "exec_type": "Template",
  "exec_params": {
    "template_id": "tpl-001",
    "template_params_json": "{\"env\":\"prod\",\"count\":3}"
  }
}
```
