#include "worker_pool.h"
#include "core/logger.h"
#include <atomic>
#include <memory>
#include "core/blocking_queue.h"

namespace taskhub {
    struct PoolStats {
        int workers;
        int queued;
        int running;
        int submitted;
        int finished;
    };
    PoolStats stats();
    
    WorkerPool* WorkerPool::instance()
    {
        static WorkerPool* instance =new WorkerPool();
        return instance;
    }

    taskhub::WorkerPool::~WorkerPool()
    {
        stop();
    }
    void WorkerPool::start(std::size_t num_workers)
    {
        if (!m_workers.empty()) return; // 已启动
        m_stopping = false;
        m_stopping = false;
        for (std::size_t i = 0; i < num_workers; ++i) {
            m_workers.emplace_back([this, i] { worker_loop(i); });
        }
        Logger::info("WorkerPool started with " + std::to_string(num_workers) + " workers");
    }
    void WorkerPool::submit(TaskPtr task)
    {
        if (!task) return;
        m_total_submitted++;
        m_queue.push(std::move(task));
    }
    void WorkerPool::submit(int TaskId)
    {
        auto optTask = TaskManager::instance().get_task(TaskId);
        if (!optTask) {
            Logger::warn("WorkerPool::submit: Task id " + std::to_string(TaskId) + " not found");
            return;
        }
        TaskPtr task = std::make_shared<Task>(*optTask);
        submit(task);
    }
    void WorkerPool::stop()
    {
        if (m_stopping.exchange(true)) {
            return; // 已经在停了
        }
        m_queue.shutdown();
        for (auto &th : m_workers) {
            if (th.joinable())
                th.join();
        }
        m_workers.clear();
        Logger::info("WorkerPool stopped");
    }

    //
    void WorkerPool::worker_loop(std::size_t worker_id)
    {
        Logger::info("Worker " + std::to_string(worker_id) + " started");
        while (!m_stopping) {
            auto optTask = m_queue.pop();
            if (!optTask) {
                // 队列关闭
                break;
            }
            TaskPtr task = *optTask;
            if (!task) continue;
            auto start = std::chrono::steady_clock::now();
            execute_one_task(worker_id, task);
            auto end = std::chrono::steady_clock::now();
            
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            Logger::info("Task " + std::to_string(task->id) + " cost = " + std::to_string(ms) + "ms");
        }
        Logger::info("Worker " + std::to_string(worker_id) + " exiting");
    }
    void WorkerPool::execute_one_task(std::size_t worker_id, TaskPtr task)
    {
         // 1) 标记为 running，更新 DB
        auto &tm = TaskManager::instance();
        task->status      = TaskStatus::Running;
        task->update_time = utils::now_string();
        tm.set_running(task->id);
    
        broadcast_task_event("task_updated", *task);
    m_current_running++;
        Logger::info("Worker " + std::to_string(worker_id)
                     + " start task " + std::to_string(task->id)
                     + ": " + task->params["cmd"].get<std::string>());
                     
        // 2) 执行命令
        std::string output;
        std::string error;
        int exit_code = 0;
        try {
            const std::string cmd = task->params["cmd"].get<std::string>();
            // TODO: 调用你已有的 TaskRunner 逻辑
            auto [exit_code, output] = utils::run_command(cmd);
            // 这里只留接口位
        } catch (const std::exception &ex) {
            exit_code = -1;
            error = ex.what();
        }
        // 3) 标记为 finished，更新 DB
        bool success = (exit_code == 0);
        task->status      = success ? TaskStatus::Success : TaskStatus::Failed;
        task->update_time = utils::now_string();
        task->exit_code   = exit_code;
        task->last_output = output;
        task->last_error  = error;
        tm.set_finished(task->id, success, exit_code, output, error);
        broadcast_task_event("task_updated", *task);
        Logger::info("Worker " + std::to_string(worker_id)
                     + " finished task " + std::to_string(task->id)
                     + ", exit=" + std::to_string(exit_code));
    }
}
