#include "worker_selector.h"
#include <algorithm>
#include <vector>

namespace taskhub::worker {
namespace {

bool queue_match(const WorkerInfo& w, const std::string& queue) {
    if (queue.empty()) {
        return true;
    }
    if (w.queues.empty()) {
        return queue == "default";
    }
    return std::find(w.queues.begin(), w.queues.end(), queue) != w.queues.end();
}

std::vector<const WorkerInfo*> collect_candidates(
    const std::unordered_map<std::string, WorkerInfo>& workers,
    const std::string& queue,
    bool includeCoolingDown)
{
    std::vector<const WorkerInfo*> candidates;
    candidates.reserve(workers.size());
    for (const auto& [id, info] : workers) {
        if (!info.isAlive()) {
            continue;
        }
        if (!queue_match(info, queue)) {
            continue;
        }
        if (info.isFull()) {
            continue;
        }
        if (!includeCoolingDown && info.isCoolingDown()) {
            continue;
        }
        candidates.push_back(&info);
    }
    return candidates;
}

void sort_candidates(std::vector<const WorkerInfo*>& candidates) {
    std::sort(candidates.begin(), candidates.end(),
              [](const WorkerInfo* a, const WorkerInfo* b) {
                  return a->id < b->id;
              });
}

} // namespace

std::optional<WorkerInfo> LeastLoadSelector::pick(
    const std::unordered_map<std::string, WorkerInfo>& workers,
    const std::string& queue)
{
    auto candidates = collect_candidates(workers, queue, false);
    if (candidates.empty()) {
        candidates = collect_candidates(workers, queue, true);
    }
    const WorkerInfo* best = nullptr;
    for (const auto* info : candidates) {
        if (!best || info->runningTasks < best->runningTasks ||
            (info->runningTasks == best->runningTasks && info->id < best->id)) {
            best = info;
        }
    }
    if (!best) {
        return std::nullopt;
    }
    return *best;
}

std::optional<WorkerInfo> RoundRobinSelector::pick(
    const std::unordered_map<std::string, WorkerInfo>& workers,
    const std::string& queue)
{
    auto candidates = collect_candidates(workers, queue, false);
    if (candidates.empty()) {
        candidates = collect_candidates(workers, queue, true);
    }
    if (candidates.empty()) {
        return std::nullopt;
    }
    sort_candidates(candidates);

    std::size_t idx = 0;
    {
        std::lock_guard<std::mutex> lk(_mutex);
        auto& cursor = _queueIndex[queue];
        if (cursor >= candidates.size()) {
            cursor = 0;
        }
        idx = cursor;
        cursor = (cursor + 1) % candidates.size();
    }
    return *candidates[idx];
}

} // namespace taskhub::worker
