#pragma once
#include "worker_info.h"
#include <optional>  // 添加这行
#include <vector>
#include <unordered_map>
#include <mutex>
#include <string>
#include <thread>
#include <memory>
#include <atomic>
#include <condition_variable>
namespace taskhub::worker {

class WorkerSelector;

// Server-side registry used by Master to track and schedule workers.
class ServerWorkerRegistry {
    public:
        static ServerWorkerRegistry& instance();
            // 不允许拷贝 / 移动
        ServerWorkerRegistry(const ServerWorkerRegistry&) = delete;
        ServerWorkerRegistry& operator=(const ServerWorkerRegistry&) = delete;
        ServerWorkerRegistry(ServerWorkerRegistry&&) = delete;
        ServerWorkerRegistry& operator=(ServerWorkerRegistry&&) = delete;
        void upsertWorker(const WorkerInfo& info);    // 注册或更新
        void removeWorker(const std::string& id);
        std::vector<WorkerInfo> listWorkers() const;
        bool touchHeartbeat(const std::string& id, int runningTasks);
        // 简单分配策略
        std::optional<WorkerInfo> pickWorkerForQueue(const std::string& queue) const;
        void markDispatchFailure(const std::string& id, std::chrono::milliseconds cooldown);

        void pruneDeadWorkers(std::chrono::milliseconds pruneAfter);
        void startSweeper(std::chrono::milliseconds sweepInterval,
                        std::chrono::milliseconds pruneAfter);
        void stopSweeper();
        
    private:
        ServerWorkerRegistry();
        void initSelector();
        mutable std::mutex _mutex;
        std::unordered_map<std::string, WorkerInfo> _workers;
        mutable std::unique_ptr<WorkerSelector> _selector;

        std::thread _sweeperThread;
        std::atomic_bool _stopSweeper{false};
        std::condition_variable _sweeperCv;
        std::chrono::milliseconds _sweepInterval{5000};
        std::chrono::milliseconds _pruneAfter{60000};
    };

}
