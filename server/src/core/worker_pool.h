#pragma once
#include "core/task_manager.h"  
#include "ws/ws_task_events.h" // broadcast_task_event
#include "core/blocking_queue.h"
#include <thread>
#include <vector>

namespace taskhub {

using TaskPtr = std::shared_ptr<Task>;

class WorkerPool {
public:
    static WorkerPool* instance();
    WorkerPool() = default;
    ~WorkerPool();
    WorkerPool(const WorkerPool&) = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;

    // 启动 N 个 worker 线程
    void start(std::size_t num_workers);

    // 提交一个任务指针到队列
    void submit(TaskPtr task);
    void submit(int TaskId); 

    // 停止线程池
    void stop();

private:
    void worker_loop(std::size_t worker_id);

    void execute_one_task(std::size_t worker_id, TaskPtr task);

private:
    BlockingQueue<TaskPtr> m_queue;
    std::vector<std::thread> m_workers;
    std::atomic<bool> m_stopping{false};
    
    std::atomic<int> m_total_submitted{0};
    std::atomic<int> m_total_finished{0};
    std::atomic<int> m_current_running{0};
};

} // namespace taskhub
