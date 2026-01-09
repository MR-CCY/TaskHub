/**
 * @file dag_thread_pool.cpp
 * @brief 实现DAG任务调度线程池
 */

#include "dag_thread_pool.h"
#include "log/logger.h"

namespace taskhub::dag {
    thread_local bool DagThreadPool::_isDagWorker = false;

    /**
     * @brief 获取DagThreadPool单例实例
     * @return 返回线程池的唯一实例
     */
    DagThreadPool &DagThreadPool::instance()
    {
        static DagThreadPool inst;
        return inst;
    }

    /**
     * @brief 启动线程池
     * @param num_workers 工作线程数量
     * 
     * 初始化指定数量的工作线程，清空任务队列，并记录启动日志
     */
    void DagThreadPool::start(std::size_t num_workers)
    {
        if (!_workers.empty()) return;
        _stopping = false;
        _maxWorkers = num_workers * 4;  // 硬上限为初始值的 4 倍，防止无限扩容
        {
            std::lock_guard<std::mutex> lk(_mtx);
            for (auto& q : _queues) {
                q.clear();
            }
            _workers.reserve(num_workers);
        }
        for (std::size_t i = 0; i < num_workers; ++i) {
            _workers.emplace_back([this, i] { worker_loop(i); });
        }
        Logger::info("DagThreadPool started with " + std::to_string(num_workers) + " workers");
    }

    /**
     * @brief 停止线程池
     * 
     * 设置停止标志，清空任务队列，通知所有工作线程退出并等待它们完成
     */
    void DagThreadPool::stop()
    {
        if (_stopping.exchange(true)) return;
        {
            std::lock_guard<std::mutex> lk(_mtx);
            for (auto& q : _queues) {
                q.clear();
            }
        }
        _cv.notify_all();
        for (auto& th : _workers) {
            if (th.joinable()) th.join();
        }
        _workers.clear();
        _activeWorkers.store(0, std::memory_order_relaxed);
        _busyWorkers.store(0, std::memory_order_relaxed);
        _maxWorkers = 0;
        Logger::info("DagThreadPool stopped");
    }

    /**
     * @brief 提交任务到线程池
     * @param job 要执行的任务函数
     * @param priority 任务优先级，默认为Normal
     * 
     * 将任务添加到对应优先级的队列中，并通知一个工作线程处理任务
     */
    void DagThreadPool::post(Job job, core::TaskPriority priority)
    {
        if (!job) return;
        std::size_t idx = priority_index(priority);
        {
            std::lock_guard<std::mutex> lk(_mtx);
            if (_stopping) return;
            _queues[idx].push_back(std::move(job));
        }
        // 尝试动态扩容，缓解嵌套 DAG 复用同池造成的饥饿/死锁
        maybeSpawnWorker(1);
        _cv.notify_one();
    }

    /**
     * @brief 析构函数，停止线程池
     */
    DagThreadPool::~DagThreadPool()
    {
        stop();
    }

    /**
     * @brief 工作线程主循环
     * @param worker_id 工作线程ID
     * 
     * 工作线程在此循环中持续获取并执行任务，直到线程池停止
     */
    void DagThreadPool::worker_loop(std::size_t worker_id)
    {
        _isDagWorker = true;
        _activeWorkers.fetch_add(1, std::memory_order_relaxed);
        Logger::info("Dag worker " + std::to_string(worker_id) + " started");
        while (true) {
            auto optJob = take_job();
            if (!optJob) break; // 队列关闭/停止
            Job job = std::move(*optJob);
            if (!job) continue;
            _busyWorkers.fetch_add(1, std::memory_order_relaxed);
            job(); 
            _busyWorkers.fetch_sub(1, std::memory_order_relaxed);
        }
        _activeWorkers.fetch_sub(1, std::memory_order_relaxed);
        _isDagWorker = false;
        Logger::info("Dag worker " + std::to_string(worker_id) + " exiting");
    }

    /**
     * @brief 从任务队列中获取一个任务
     * @return 返回一个可选的任务，如果线程池停止且无任务则返回nullopt
     * 
     * 按照优先级顺序(Critical > High > Normal > Low)从队列中获取任务，
     * 如果没有任务则等待直到有任务或线程池停止
     */
    std::optional<DagThreadPool::Job> DagThreadPool::take_job()
    {
        static const std::array<core::TaskPriority, 4> order = {
            core::TaskPriority::Critical,
            core::TaskPriority::High,
            core::TaskPriority::Normal,
            core::TaskPriority::Low
        };

        std::unique_lock<std::mutex> lk(_mtx);
        _cv.wait(lk, [this] {
            return _stopping || has_jobs_locked();
        });

        if (_stopping && !has_jobs_locked()) {
            return std::nullopt;
        }

        for (auto pri : order) {
            auto& q = _queues[priority_index(pri)];
            if (!q.empty()) {
                Job job = std::move(q.front());
                q.pop_front();
                return job;
            }
        }
        return std::nullopt;
    }

    /**
     * @brief 检查是否有待处理的任务
     * @return 如果有任何队列中有任务则返回true，否则返回false
     * 
     * 此函数必须在持有_mtx锁的情况下调用
     */
    bool DagThreadPool::has_jobs_locked() const
    {
        for (const auto& q : _queues) {
            if (!q.empty()) return true;
        }
        return false;
    }

    /**
     * @brief 根据任务优先级获取队列索引
     * @param p 任务优先级
     * @return 返回对应的队列索引(0-3)
     * 
     * Critical -> 0, High -> 1, Normal -> 2, Low -> 3
     */
    std::size_t DagThreadPool::priority_index(core::TaskPriority p) const
    {
        switch (p) {
        case core::TaskPriority::Critical: return 0;
        case core::TaskPriority::High:     return 1;
        case core::TaskPriority::Normal:   return 2;
        case core::TaskPriority::Low:      return 3;
        default:                           return 2;
        }
    }

    bool DagThreadPool::isDagWorkerThread() const
    {
        return _isDagWorker;
    }

    void DagThreadPool::maybeSpawnWorker(std::size_t minSpare)
    {
        if (_stopping.load(std::memory_order_relaxed)) return;

        std::unique_lock<std::mutex> lk(_mtx);

        // spare = 可用线程（总线程数 - 忙碌线程数）
        auto busy   = _busyWorkers.load(std::memory_order_relaxed);
        std::size_t total = _workers.size();
        std::size_t spare = (total > busy) ? (total - busy) : 0;

        // 检查是否已达上限
        if (total >= _maxWorkers) {
            return;
        }

        // 计算队列中的任务数
        std::size_t queuedJobs = 0;
        for (const auto& q : _queues) {
            queuedJobs += q.size();
        }

        // 策略：如果有排队的任务，且空闲线程不足，则扩容
        // 这对于并行嵌套 DAG 很重要：即使当前有空闲线程，
        // 但如果有多个任务排队，这些线程可能会被长时间占用（同步执行嵌套 DAG）
        if (queuedJobs > 0 && spare < queuedJobs) {
            const std::size_t newId = _workers.size();
            _workers.emplace_back([this, newId] { worker_loop(newId); });
            Logger::info("DagThreadPool dynamic spawn worker " + std::to_string(newId) +
                         ", total=" + std::to_string(_workers.size()) +
                         ", busy=" + std::to_string(busy) +
                         ", queued=" + std::to_string(queuedJobs) +
                         ", maxWorkers=" + std::to_string(_maxWorkers));
            _cv.notify_one();
        }
    }
}
