#include "worker_registry.h"
#include <algorithm>  // std::find
namespace taskhub::worker {
    WorkerRegistry& WorkerRegistry::instance()
    {
        // TODO: 在此处插入 return 语句
        static WorkerRegistry instance;
        return instance;
    }

    void WorkerRegistry::upsertWorker(const WorkerInfo &info)
    {
        std::lock_guard<std::mutex> lk(_mutex);
        _workers[info.id] = info;
    }

    void WorkerRegistry::removeWorker(const std::string &id)
    {
        std::lock_guard<std::mutex> lk(_mutex);
        _workers.erase(id);
    }

    std::vector<WorkerInfo> WorkerRegistry::listWorkers() const
    {
        std::lock_guard<std::mutex> lk(_mutex);
        std::vector<WorkerInfo> result;
        result.reserve(_workers.size());
        for (const auto& [id, info] : _workers) {
            result.push_back(info);
        }
        return result;
    }

    std::optional<WorkerInfo> WorkerRegistry::pickWorkerForQueue(const std::string &queue) const
    {
        std::lock_guard<std::mutex> lk(_mutex);

        const WorkerInfo* best = nullptr;

        auto queueMatch = [&queue](const WorkerInfo& w) -> bool {
            // 没有 queue 限制：任何队列都行
            if (queue.empty()) {
                return true;
            }
            // worker 没填 queues，视为默认队列 "default"
            if (w.queues.empty()) {
                return (queue == "default");
            }
            // 显式匹配
            return std::find(w.queues.begin(), w.queues.end(), queue) != w.queues.end();
        };

        for (auto& [id, info] : _workers) {
            // 要在线
            if (!info.isAlive()) {
                continue;
            }
            // 要匹配队列
            if (!queueMatch(info)) {
                continue;
            }

            if (!best || info.runningTasks < best->runningTasks) {
                best = &info;
            }
        }

        if (!best) {
            return std::nullopt;
        }
        // 拷贝一份出去，避免把内部引用暴露给外部
        return *best;
    }
}