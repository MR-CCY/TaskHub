#include "remote_strategy.h"
#include <json.hpp>
#include <httplib.h>
#include "worker/worker_registry.h"
#include "worker/worker_info.h"
#include "runner/task_config.h"
#include "runner/task_result.h"
#include "core/logger.h"
#include <chrono>

using json = nlohmann::json;

namespace taskhub::runner {
    core::TaskResult RemoteExecutionStrategy::execute(const core::TaskConfig &cfg, std::atomic_bool *cancelFlag, Deadline deadline)
    {
        core::TaskResult result;

        // 0) 取消/超时前置检查
        if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
            result.status  = core::TaskStatus::Canceled;
            result.message = "RemoteExecution: canceled before dispatch";
            return result;
        }
        if (deadline != Deadline::max() && SteadyClock::now() >= deadline) {
            result.status  = core::TaskStatus::Timeout;
            result.message = "RemoteExecution: timeout before dispatch";
            return result;
        }

        // TODO（M11 使用）：向远程 Worker 发送任务：
        //   - 根据 cfg.metadata / queue / priority 选择 Worker
        //   - 通过 HTTP / RPC / 消息队列投递任务
        //   - 等待 Worker 回调 / 查询状态
        //   - 支持取消时，发 cancel 请求给 Worker
        //
        // 目前你可以先占位，返回 Failed 或 NotImplemented
         // 1. 选择队列（如果没填 queue，用默认的）
        std::string queue = cfg.queue.empty() ? "default" : cfg.queue;
            // 2. 从 WorkerRegistry 里选一个 Worker
        auto optWorker = worker::WorkerRegistry::instance().pickWorkerForQueue(queue);
        if (!optWorker.has_value()) {
            result.status  = core::TaskStatus::Failed;
            result.message = "RemoteExecution: no available worker for queue=" + queue;
            Logger::error(result.message);
            return result;
        }
        worker::WorkerInfo worker = *optWorker; // 拷贝一份，避免锁问题
        result.workerId   = worker.id;
        result.workerHost = worker.host;
        result.workerPort = worker.port;

        auto innerCfg = makeInnerTask(cfg);
        json jReq = buildRequestJson(innerCfg);
        const std::string body = jReq.dump();
           // 3. 构造 HTTP 客户端
        httplib::Client cli(worker.host, worker.port);
        // connect/write 给一个合理上限；read 尽量受 deadline 约束
        cli.set_connection_timeout(3, 0); // 3s connect
        cli.set_write_timeout(10, 0);     // 10s send

        if (deadline != Deadline::max()) {
            const auto now = SteadyClock::now();
            if (deadline <= now) {
                result.status  = core::TaskStatus::Timeout;
                result.message = "RemoteExecution: timeout before http call";
                Logger::error(result.message);
                return result;
            }
            auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
            if (remain.count() <= 0) {
                remain = std::chrono::milliseconds(1);
            }
            const int sec  = static_cast<int>(remain.count() / 1000);
            const int usec = static_cast<int>((remain.count() % 1000) * 1000);
            cli.set_read_timeout(sec, usec);
        } else {
            cli.set_read_timeout(60, 0); // 无 deadline 时兜底
        }

           // 4. 发送请求
        static const char* kWorkerExecutePath = "/api/worker/execute";

        Logger::info(std::string("RemoteExecution: POST ") + kWorkerExecutePath +
                    " -> " + worker.host + ":" + std::to_string(worker.port) +
                    ", workerId=" + worker.id +
                    ", taskId=" + cfg.id.value);
        const auto start = SteadyClock::now();
        auto resp = cli.Post(kWorkerExecutePath, body, "application/json");
        const auto end = SteadyClock::now();
        result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        if (!resp) {
            // best-effort：根据 cancel/deadline 推断失败类型
            if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
                result.status  = core::TaskStatus::Canceled;
                result.message = "RemoteExecution: canceled during http call";
            } else if (deadline != Deadline::max() && SteadyClock::now() >= deadline) {
                result.status  = core::TaskStatus::Timeout;
                result.message = "RemoteExecution: timeout during http call";
            } else {
                result.status  = core::TaskStatus::Failed;
                result.message = "RemoteExecution: HTTP failed (no response)";
            }
            Logger::error(result.message);
            return result;
        }
    
        if (resp->status != 200) {
            result.status     = core::TaskStatus::Failed;
            result.message    = "RemoteExecution: HTTP status=" + std::to_string(resp->status);
            result.stderrData = resp->body; // 把 body 当错误信息
            Logger::error(result.message);
            return result;
        }
    
        // 7) parse json（宽松解析，避免异常）
        json jResp = json::parse(resp->body, nullptr, false);
        if (jResp.is_discarded()) {
            result.status     = core::TaskStatus::Failed;
            result.message    = "RemoteExecution: invalid JSON response";
            result.stderrData = resp->body;
            Logger::error(result.message);
            return result;
        }
        Logger::debug("RemoteExecution: got response: " + resp->body);  
        // 8) map to TaskResult
        core::TaskResult parsed = core::parseResultJson(jResp);

        // 9) keep worker info (avoid losing them if parse doesn't set)
        parsed.workerId    = worker.id;
        parsed.workerHost  = worker.host;
        parsed.workerPort  = worker.port;
        // 若 worker 侧没返回 duration，这里用我们本次 HTTP 调用耗时兜底
        if (parsed.durationMs <= 0) {
            parsed.durationMs = result.durationMs;
        }

        return parsed;
    }
}