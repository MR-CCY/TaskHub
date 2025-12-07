// src/dag/dag_thread_pool.h
#pragma once
#include <functional>
#include <thread>
#include <vector>
#include <atomic>
#include "core/blocking_queue.h"

namespace taskhub::dag {

class DagThreadPool {
public:
    using Job = std::function<void()>;

    static DagThreadPool& instance();

    void start(std::size_t num_workers);
    void stop();

    void post(Job job);  // DAG 这边只需要这个

private:
    DagThreadPool() = default;
    ~DagThreadPool();

    void worker_loop(std::size_t worker_id);

    std::vector<std::thread>         _workers;
    BlockingQueue<Job>         _queue;
    std::atomic_bool                 _stopping{false};
};

} // namespace taskhub::dag