#include "worker_heartbeat_client.h"
#include <utility>
#include <json.hpp>
#include <httplib.h>
#include "log/logger.h"
#include "utils/utils.h"
#include <vector>
#include "core/config.h"

using json = nlohmann::json;

namespace taskhub::worker {
void WorkerHeartbeatClient::start(std::string masterHost,
                                  int masterPort,
                                  std::string workerId,
                                  std::string workerHost,
                                  int workerPort,
                                  std::vector<std::string> queues,
                                  std::vector<std::string> labels,
                                  std::function<int()> getRunningTasks,
                                  std::chrono::milliseconds interval)
{
    _masterHost      = std::move(masterHost);
    _masterPort      = masterPort;
    _workerId        = std::move(workerId);
    _workerHost      = std::move(workerHost);
    _workerPort      = workerPort;
    _queues          = std::move(queues);
    _labels          = std::move(labels);
    _getRunningTasks = std::move(getRunningTasks);
    _interval        = interval;
    _maxRunningTasks = taskhub::Config::instance().get("work.max_running_tasks", 1);
    if (_maxRunningTasks <= 0) {
        _maxRunningTasks = 1;
    }

    Logger::info("WorkerHeartbeatClient configured: master=" +
                 _masterHost + ":" + std::to_string(_masterPort) +
                 ", worker_id=" + _workerId +
                 ", worker=" + _workerHost + ":" + std::to_string(_workerPort) +
                 ", interval=" + std::to_string(_interval.count()) + "ms" +
                 ", max_running_tasks=" + std::to_string(_maxRunningTasks));

    _stop.store(false, std::memory_order_release);
    _registered.store(false, std::memory_order_release);

    _th = std::thread(&WorkerHeartbeatClient::loop, this);
}

void WorkerHeartbeatClient::stop()
{
    _stop.store(true, std::memory_order_release);
    if (_th.joinable()) {
        _th.join();
    }
}

void WorkerHeartbeatClient::loop()
{
    httplib::Client client(_masterHost, _masterPort);
    client.set_connection_timeout(2, 0);
    client.set_read_timeout(5, 0);
    client.set_write_timeout(5, 0);

    auto do_register = [&]() -> bool {
        json j;
        j["id"]            = _workerId;
        j["host"]          = _workerHost;
        j["port"]          = _workerPort;
        j["queues"]        = _queues;
        j["labels"]        = _labels;
        j["running_tasks"] = _getRunningTasks ? _getRunningTasks() : 0;
        j["max_running_tasks"] = _maxRunningTasks;

        auto resp = client.Post("/api/workers/register", j.dump(), "application/json");
        if (!resp || resp->status >= 300) {
            const std::string status = resp ? std::to_string(resp->status) : "no response";
            Logger::warn("Worker register failed: " + status);
            return false;
        }
        Logger::info("Worker register succeeded: " + utils::now_string());
        return true;
    };

    auto do_heartbeat = [&]() -> std::optional<int> {
        json j;
        j["id"]            = _workerId;
        j["running_tasks"] = _getRunningTasks ? _getRunningTasks() : 0;

        auto resp = client.Post("/api/workers/heartbeat", j.dump(), "application/json");
        if (!resp) {
            Logger::warn("Worker heartbeat failed: no response");
            return std::nullopt;
        }
        if (resp->status >= 300) {
            Logger::warn("Worker heartbeat failed: " + std::to_string(resp->status));
            return resp->status;
        }
        Logger::info("Worker heartbeat succeeded: " + utils::now_string());
        return resp->status;
    };

    // 先注册，再进入心跳循环
    while (!_stop.load(std::memory_order_acquire)) {
        if (!_registered.load(std::memory_order_acquire)) {
            if (do_register()) {
                _registered.store(true, std::memory_order_release);
            } else {
                // Master 未就绪/网络问题：稍等再试
                std::this_thread::sleep_for(_interval);
                continue;
            }
        }

        auto hb = do_heartbeat();
        if (!hb.has_value()) {
            // no response：通常是 master 不可达，先保持 registered=true，下一轮再试心跳
        } else if (*hb == 404) {
            // 你说 master 侧“未注册则 404”，那这里自动补一次 register
            _registered.store(false, std::memory_order_release);
        }

        if (_stop.load(std::memory_order_acquire)) {
            break;
        }
        std::this_thread::sleep_for(_interval);
    }
}
}
