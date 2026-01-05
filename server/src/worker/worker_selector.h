#pragma once
#include <optional>
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include "worker_info.h"

namespace taskhub::worker {

class WorkerSelector {
public:
    virtual ~WorkerSelector() = default;
    virtual std::optional<WorkerInfo> pick(const std::unordered_map<std::string, WorkerInfo>& workers,
                                           const std::string& queue) = 0;
    virtual const char* name() const = 0;
};

class LeastLoadSelector : public WorkerSelector {
public:
    std::optional<WorkerInfo> pick(const std::unordered_map<std::string, WorkerInfo>& workers,
                                   const std::string& queue) override;
    const char* name() const override { return "least-load"; }
};

class RoundRobinSelector : public WorkerSelector {
public:
    std::optional<WorkerInfo> pick(const std::unordered_map<std::string, WorkerInfo>& workers,
                                   const std::string& queue) override;
    const char* name() const override { return "rr"; }

private:
    std::mutex _mutex;
    std::unordered_map<std::string, std::size_t> _queueIndex;
};

} // namespace taskhub::worker
