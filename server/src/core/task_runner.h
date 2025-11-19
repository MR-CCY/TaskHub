// core/task_runner.h
#pragma once
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "task.h"

namespace taskhub {

class TaskRunner {
public:
    static TaskRunner& instance();

    void start();              // 启动后台线程
    void stop();               // （可选，先留 TODO）

    void enqueue(Task::IdType id);  // 有新任务时调用

private:
    TaskRunner() = default;
    TaskRunner(const TaskRunner&) = delete;
    TaskRunner& operator=(const TaskRunner&) = delete;

    void worker_loop();        // 后台线程主循环

private:
    std::thread              m_thread;
    std::queue<Task::IdType> m_queue;
    std::mutex               m_mutex;
    std::condition_variable  m_cv;
    bool                     m_stopping{false};
    bool                     m_started{false};
};

} // namespace taskhub