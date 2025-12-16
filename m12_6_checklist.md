M12.6 回归验收 Checklist（直接可跑）

准备
	1.	启动 server（8082）+ ws（8090）+ worker（8083）
	2.	打开你的 HTML WS 测试页，连上：ws://127.0.0.1:8090/ws

⸻

CASE 1：Shell stdout/stderr（HTTP + WS 同时验收）

1.1 WS 订阅日志

在 HTML 里发送：

{"op":0,"topic":0,"task_id":"log_test_1"}

1.2 触发任务（Shell）

curl -X POST http://localhost:8082/api/dag/run \
  -H "Content-Type: application/json" \
  -d '{
    "tasks": [
      { "id": "log_test_1", "exec_type": "Shell", "exec_command": "echo S1; echo S2; echo SE1 1>&2; echo SE2 1>&2" }
    ]
  }'

1.3 HTTP 拉取校验

curl "http://localhost:8082/api/tasks/logs?task_id=log_test_1&from=1&limit=200"

✅ 你应当同时看到：
	•	WS 里出现 stream=1 的 S1/S2，stream=2 的 SE1/SE2
	•	HTTP records 里同样能分页拉到，并且 seq 递增

⸻

CASE 2：Remote stdout/stderr 回写（核心）

2.1 WS 订阅日志

{"op":0,"topic":0,"task_id":"log_test_1"}

2.2 触发 Remote（inner shell）

curl -X POST http://localhost:8082/api/dag/run \
  -H "Content-Type: application/json" \
  -d '{
    "tasks": [
      {
        "id": "log_test_1",
        "exec_type": "Remote",
        "queue": "default",
        "exec_params": {
          "inner.exec_type": "Shell",
          "inner.exec_command": "echo R1; echo R2; echo RE1 1>&2; echo RE2 1>&2"
        }
      }
    ]
  }'

2.3 验收点

✅ WS/HTTP 都应该出现：
	•	R1/R2 在 stream=1
	•	RE1/RE2 在 stream=2
	•	并且能看到 Remote 的 event/dispatch 相关 Event log（你现在 logs 里已经有 “dispatch start/picked worker/http finished/end” 那套）

⸻

CASE 3：TaskEvents（topic=1）+ DagEvents 组合验收

3.1 WS 订阅事件

{"op":0,"topic":1,"task_id":"log_test_1"}

3.2 再跑一次 Shell 或 Remote（任意）

✅ WS 应该收到类似：
	•	task_start / attempt_start / attempt_end / task_end
	•	DAG 相关：dag_node_ready / dag_node_start / dag_node_end
并且 type 为 "event"，字段里有 event + extra + task_id

⸻

CASE 4：分页/增量拉取（HTTP）
	1.	先拉小分页：

curl "http://localhost:8082/api/tasks/logs?task_id=log_test_1&from=1&limit=2"

拿到 next_from=N
	2.	再拉下一页：

curl "http://localhost:8082/api/tasks/logs?task_id=log_test_1&from=N&limit=200"

✅ 验收点：不会重复、不会跳号（seq 连续递增，至少同一次运行内是连续的）

⸻

CASE 5：落盘验证
	1.	跑一个大输出任务（Shell 或 Remote 都行）
	2.	检查：

	•	./logs/taskhub.log 变大
	•	轮转文件出现（你已经有 6 个了）
	•	“最新写入”一定在当前 taskhub.log，轮转文件内容是历史段

⸻