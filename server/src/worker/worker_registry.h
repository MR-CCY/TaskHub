#pragma once
#include "worker_info.h"
#include <optional>  // 添加这行
#include <vector>
#include <unordered_map>
#include <mutex>
#include <string>

namespace taskhub::worker {

class WorkerRegistry {
    public:
        static WorkerRegistry& instance();
            // 不允许拷贝 / 移动
        WorkerRegistry(const WorkerRegistry&) = delete;
        WorkerRegistry& operator=(const WorkerRegistry&) = delete;
        WorkerRegistry(WorkerRegistry&&) = delete;
        WorkerRegistry& operator=(WorkerRegistry&&) = delete;
        void upsertWorker(const WorkerInfo& info);    // 注册或更新
        void removeWorker(const std::string& id);
        std::vector<WorkerInfo> listWorkers() const;
    
        // 简单分配策略（M11 用最小版）
        std::optional<WorkerInfo> pickWorkerForQueue(const std::string& queue) const;
    
    private:
        WorkerRegistry() = default; 
        mutable std::mutex _mutex;
        std::unordered_map<std::string, WorkerInfo> _workers;
    };

}