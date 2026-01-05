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

    void start(std::size_t num_workers);
    void stop();

    void post(Job job, core::TaskPriority priority = core::TaskPriority::Normal);  // DAG 这边只需要这个

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
};

} // namespace taskhub::dag
