#include "dag_thread_pool.h"
#include "core/logger.h"
namespace taskhub::dag {
    DagThreadPool &DagThreadPool::instance()
    {
        // TODO: 在此处插入 return 语句
        static DagThreadPool inst;
        return inst;
    }

    void DagThreadPool::start(std::size_t num_workers)
    {
        if (!_workers.empty()) return;
        _stopping = false;
        for (std::size_t i = 0; i < num_workers; ++i) {
            _workers.emplace_back([this, i] { worker_loop(i); });
        }
        Logger::info("DagThreadPool started with " + std::to_string(num_workers) + " workers");
    }

    void DagThreadPool::stop()
    {
        if (_stopping.exchange(true)) return;
        _queue.shutdown();
        for (auto& th : _workers) {
            if (th.joinable()) th.join();
        }
        _workers.clear();
        Logger::info("DagThreadPool stopped");
    }

    void DagThreadPool::post(Job job)
    {
        if (!job) return;
        _queue.push(std::move(job));
    }

    DagThreadPool::~DagThreadPool()
    {
        stop();
    }

    void DagThreadPool::worker_loop(std::size_t worker_id)
    {
        Logger::info("Dag worker " + std::to_string(worker_id) + " started");
        while (!_stopping) {
            auto optJob = _queue.pop();
            if (!optJob) break; // 队列关闭
            Job job = std::move(*optJob);
            if (!job) continue;
            job();  // ★ 就一行：跑这个 lambda
        }
        Logger::info("Dag worker " + std::to_string(worker_id) + " exiting");
    }
}