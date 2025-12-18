#include "worker_registry.h"
#include <algorithm>  // std::find
#include "log/logger.h"
#include "utils/utils.h"

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

        WorkerInfo copy = info;
    
        // ✅ 注册即视为一次心跳
        copy.lastHeartbeat = std::chrono::steady_clock::now();
    
        _workers[copy.id] = std::move(copy);
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

    bool WorkerRegistry::touchHeartbeat(const std::string &id, int runningTasks)
    {
        std::lock_guard<std::mutex> lk(_mutex);

        auto it = _workers.find(id);
        if (it == _workers.end()) {
            return false;
        }
    
        it->second.lastHeartbeat = std::chrono::steady_clock::now();
        it->second.runningTasks = runningTasks;
        return true;
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
    /**
     * @brief 清理已死亡的工作进程
     * 
     * 该函数会遍历所有已注册的工作进程，检查它们的存活状态。
     * 对于已死亡且超过指定时间未发送心跳的工作进程，将从注册表中移除。
     * 
     * @param pruneAfter 指定工作进程死亡后等待清理的时间阈值
     */
    void WorkerRegistry::pruneDeadWorkers(std::chrono::milliseconds pruneAfter)
    {
        std::lock_guard<std::mutex> lk(_mutex);
        Logger::debug("Pruning dead workers...");

        const auto now = std::chrono::steady_clock::now();

        for (auto it = _workers.begin(); it != _workers.end(); ) {
            auto& info = it->second;

            // 还活着就跳过
            if (info.isAlive()) {
                ++it;
                continue;
            }

            // 已经不活跃：超过 pruneAfter 就真正移除
            const auto deadFor = std::chrono::duration_cast<std::chrono::milliseconds>(now - info.lastHeartbeat);
            if (deadFor > pruneAfter) {
                Logger::debug("Removing dead worker: " + info.id + ", deadForMs=" + std::to_string(deadFor.count()));
                it = _workers.erase(it);
                continue;
            }

            ++it;
        }
    }
    /**
     * 启动清理线程，定期清理已死亡的工作进程
     * 
     * @param sweepInterval 清理间隔时间，每隔此时间执行一次清理操作
     * @param pruneAfter 判断工作进程死亡的时间阈值，超过此时间未响应的工作进程将被清理
     */
    void WorkerRegistry::startSweeper(std::chrono::milliseconds sweepInterval, std::chrono::milliseconds pruneAfter)
    {
        // 如果已经启动过，先停掉再启动，避免重复线程
        stopSweeper();

        _stopSweeper = false;
        _sweeperThread = std::thread([this, sweepInterval, pruneAfter]() {
            while (!_stopSweeper.load(std::memory_order_acquire)) {
                pruneDeadWorkers(pruneAfter);

                // 分段 sleep：便于 stopSweeper() 更快生效
                const auto slice = std::chrono::milliseconds(200);
                auto slept = std::chrono::milliseconds(0);
                while (slept < sweepInterval && !_stopSweeper.load(std::memory_order_acquire)) {
                    const auto step = (sweepInterval - slept > slice) ? slice : (sweepInterval - slept);
                    std::this_thread::sleep_for(step);
                    slept += step;
                }
            }
        });
    }
        /**
         * @brief 停止清理线程
         * 
         * 该函数用于停止后台的清理线程。首先设置停止标志位，然后等待清理线程结束。
         * 如果清理线程正在运行且可连接，则等待其执行完毕。
         */
        void WorkerRegistry::stopSweeper()
        {
            _stopSweeper.store(true, std::memory_order_release);
            if (_sweeperThread.joinable()) {
                _sweeperThread.join();
            }
        }
}