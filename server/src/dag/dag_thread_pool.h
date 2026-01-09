// src/dag/dag_thread_pool.h
#pragma once
#include <functional>
#include <thread>
#include <vector>
#include <atomic>
#include <array>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <optional>
#include "runner/task_config.h"

namespace taskhub::dag {

class DagThreadPool {
public:
    using Job = std::function<void()>;

    static DagThreadPool& instance();
    bool isDagWorkerThread() const;

    void start(std::size_t num_workers);
    void stop();

    void post(Job job, core::TaskPriority priority = core::TaskPriority::Normal);  // DAG 这边只需要这个
    // 动态扩容：在嵌套 DAG 情况下，池内线程全被占用时自动拉起新的 worker
    void maybeSpawnWorker(std::size_t minSpare = 1);

private:
    DagThreadPool() = default;
    ~DagThreadPool();

    void worker_loop(std::size_t worker_id);
    std::optional<Job> take_job();
    bool has_jobs_locked() const;
    std::size_t priority_index(core::TaskPriority p) const;

    std::vector<std::thread> _workers;
    std::array<std::deque<Job>, 4> _queues; // Critical, High, Normal, Low
    std::mutex _mtx;
    std::condition_variable _cv;
    std::atomic_bool _stopping{false};
    std::atomic<std::size_t> _activeWorkers{0}; // 线程总数（存活）
    std::atomic<std::size_t> _busyWorkers{0};   // 正在执行任务的线程数
    std::size_t _maxWorkers{0};
    static thread_local bool _isDagWorker;
};

} // namespace taskhub::dag
