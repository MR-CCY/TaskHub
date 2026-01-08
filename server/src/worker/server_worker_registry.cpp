#include "server_worker_registry.h"
#include <cctype>
#include "log/logger.h"
#include "worker_selector.h"
#include "core/config.h"

namespace taskhub::worker {
namespace {
    /**
     * @brief 将策略名称标准化为小写格式
     * 
     * @param value 输入的策略名称字符串
     * @return std::string 标准化后的小写策略名称，如果输入为空则返回"least-load"
     */
    std::string normalize_strategy_name(const std::string& value) {
        if (value.empty()) return "least-load";
        std::string s(value);
        for (auto& ch : s) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        return s;
    }
}

    /**
     * @brief 获取ServerWorkerRegistry单例实例
     * 
     * @return ServerWorkerRegistry& 单例实例的引用
     */
    ServerWorkerRegistry& ServerWorkerRegistry::instance()
    {
        static ServerWorkerRegistry instance;
        return instance;
    }

    /**
     * @brief 构造ServerWorkerRegistry实例，初始化选择器
     */
    ServerWorkerRegistry::ServerWorkerRegistry()
    {
        initSelector();
    }

    /**
     * @brief 初始化工作进程选择策略
     * 
     * 从配置中读取工作进程选择策略，默认为"least-load"，
     * 根据配置创建相应的选择器实例（RoundRobinSelector或LeastLoadSelector）
     */
    void ServerWorkerRegistry::initSelector()
    {
        const std::string configured = taskhub::Config::instance().get<std::string>("worker.select_strategy", "least-load");
        const std::string strategy = normalize_strategy_name(configured);
        if (strategy == "rr" || strategy == "round-robin" || strategy == "round_robin") {
            _selector = std::make_unique<RoundRobinSelector>();
        } else {
            if (!configured.empty() && strategy != "least-load" && strategy != "least_load") {
                Logger::warn("Unknown worker select strategy: " + configured + ", fallback=least-load");
            }
            _selector = std::make_unique<LeastLoadSelector>();
        }
        Logger::info("ServerWorkerRegistry selector strategy: " + std::string(_selector->name()));
    }

    /**
     * @brief 更新或插入工作进程信息
     * 
     * 将工作进程信息添加到注册表中，如果已存在则更新，同时更新心跳时间
     * 
     * @param info 工作进程信息
     */
    void ServerWorkerRegistry::upsertWorker(const WorkerInfo &info)
    {
        {
            std::lock_guard<std::mutex> lk(_mutex);

            WorkerInfo copy = info;
        
            // ✅ 注册即视为一次心跳
            copy.lastHeartbeat = std::chrono::steady_clock::now();
        
            _workers[copy.id] = std::move(copy);
        }
        _sweeperCv.notify_one();
    }

    /**
     * @brief 从注册表中移除指定ID的工作进程
     * 
     * @param id 要移除的工作进程ID
     */
    void ServerWorkerRegistry::removeWorker(const std::string &id)
    {
        std::lock_guard<std::mutex> lk(_mutex);
        _workers.erase(id);
    }

    /**
     * @brief 获取所有注册的工作进程列表
     * 
     * @return std::vector<WorkerInfo> 工作进程信息列表的副本
     */
    std::vector<WorkerInfo> ServerWorkerRegistry::listWorkers() const
    {
        std::lock_guard<std::mutex> lk(_mutex);
        std::vector<WorkerInfo> result;
        result.reserve(_workers.size());
        for (const auto& [id, info] : _workers) {
            result.push_back(info);
        }
        return result;
    }

    /**
     * @brief 更新指定工作进程的心跳时间和运行任务数
     * 
     * @param id 工作进程ID
     * @param runningTasks 当前运行的任务数
     * @return bool 如果找到对应ID的工作进程并更新成功返回true，否则返回false
     */
    bool ServerWorkerRegistry::touchHeartbeat(const std::string &id, int runningTasks)
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

    /**
     * @brief 为指定队列选择一个合适的工作进程
     * 
     * @param queue 队列名称
     * @return std::optional<WorkerInfo> 选中的工作进程信息，如果没有选择器则返回nullopt
     */
    std::optional<WorkerInfo> ServerWorkerRegistry::pickWorkerForQueue(const std::string &queue) const
    {
        std::lock_guard<std::mutex> lk(_mutex);
        if (!_selector) {
            return std::nullopt;
        }
        return _selector->pick(_workers, queue);
    }

    /**
     * @brief 标记工作进程调度失败并设置冷却时间
     * 
     * @param id 工作进程ID
     * @param cooldown 冷却时间
     */
    void ServerWorkerRegistry::markDispatchFailure(const std::string &id, std::chrono::milliseconds cooldown)
    {
        std::lock_guard<std::mutex> lk(_mutex);
        auto it = _workers.find(id);
        if (it == _workers.end()) {
            return;
        }
        it->second.dispatchCooldownUntil = std::chrono::steady_clock::now() + cooldown;
    }
    /**
     * @brief 清理已死亡的工作进程
     * 
     * 该函数会遍历所有已注册的工作进程，检查它们的存活状态。
     * 对于已死亡且超过指定时间未发送心跳的工作进程，将从注册表中移除。
     * 
     * @param pruneAfter 指定工作进程死亡后等待清理的时间阈值
     */
    void ServerWorkerRegistry::pruneDeadWorkers(std::chrono::milliseconds pruneAfter)
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
    void ServerWorkerRegistry::startSweeper(std::chrono::milliseconds sweepInterval, std::chrono::milliseconds pruneAfter)
    {
        // 如果已经启动过，先停掉再启动，避免重复线程
        stopSweeper();

        _stopSweeper = false;
        _sweepInterval = sweepInterval;
        _pruneAfter = pruneAfter;
        _sweeperThread = std::thread([this]() {
            while (!_stopSweeper.load(std::memory_order_acquire)) {
                bool hasWorkers = false;
                {
                    std::unique_lock<std::mutex> lk(_mutex);
                    if (_workers.empty()) {
                        _sweeperCv.wait_for(lk, _sweepInterval, [this]() {
                            return _stopSweeper.load(std::memory_order_acquire) || !_workers.empty();
                        });
                    }
                    hasWorkers = !_workers.empty();
                }

                if (_stopSweeper.load(std::memory_order_acquire)) {
                    break;
                }
                if (hasWorkers) {
                    pruneDeadWorkers(_pruneAfter);
                } else {
                    continue;
                }

                // 分段 sleep：便于 stopSweeper() 更快生效
                const auto slice = std::chrono::milliseconds(200);
                auto slept = std::chrono::milliseconds(0);
                while (slept < _sweepInterval && !_stopSweeper.load(std::memory_order_acquire)) {
                    const auto step = (_sweepInterval - slept > slice) ? slice : (_sweepInterval - slept);
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
        void ServerWorkerRegistry::stopSweeper()
        {
            _stopSweeper.store(true, std::memory_order_release);
            _sweeperCv.notify_all();
            if (_sweeperThread.joinable()) {
                _sweeperThread.join();
            }
        }
}
