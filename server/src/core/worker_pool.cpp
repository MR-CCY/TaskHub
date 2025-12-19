#include "worker_pool.h"
#include "log/logger.h"
#include <atomic>
#include <memory>
#include "core/blocking_queue.h"
#include "utils/utils_exec.h"
#include "utils/utils.h"
namespace taskhub {
    WorkerPool* WorkerPool::instance()
    {
        // Use function-local static to ensure destruction happens at program exit
        // so worker threads get joined and resources released.
        static WorkerPool instance;
        return &instance;
    }

    taskhub::WorkerPool::~WorkerPool()
    {
        stop();
    }
    void WorkerPool::start(std::size_t num_workers)
    {
        if (!m_workers.empty()) return; // 已启动
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
        auto task = TaskManager::instance().get_task_ptr(TaskId);
        if (!task) {
            Logger::warn("WorkerPool::submit: Task id " + std::to_string(TaskId) + " not found");
            return;
        }
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

    PoolStats WorkerPool::stats() const
    {
        PoolStats s;
        s.workers   = static_cast<int>(m_workers.size());
        s.queued    = m_queue.size();
        s.running   = m_current_running.load();
        s.submitted = m_total_submitted.load();
        s.finished  = m_total_finished.load();
        return s;
    }

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
        auto &tm = TaskManager::instance();
         // 0) 执行前，再检查一下是否被取消
        if (task->cancel_flag) {
            task->status      = TaskStatus::Canceled;
            task->update_time = utils::now_string();
            tm.updateTask(*task);
            broadcast_task_event("task_updated", *task);

            Logger::info("Worker " + std::to_string(worker_id) +
                        " skip canceled task " + std::to_string(task->id));
            return;
        }

         // 1) 标记为 running，更新 DB
       
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
        const std::string cmd = task->params["cmd"].get<std::string>();

        int max_retries = task->max_retries;
        int attempt     = 0;
        bool final_timeout   = false;
        bool final_canceled  = false;
        int  final_exit_code = 0;
        std::string final_out, final_err;

        while (true) {
            // 每次尝试前再检查是否取消了
            if (task->cancel_flag) {
                final_canceled = true;
                break;
            }
            Logger::info("Worker " + std::to_string(worker_id) +
            " attempt " + std::to_string(attempt+1) +
            " for task " + std::to_string(task->id));

            auto start = std::chrono::steady_clock::now();
            ExecResult r = run_with_timeout(cmd, task->timeout_sec);

            auto end   = std::chrono::steady_clock::now();
            auto ms    = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

            Logger::info("Task " + std::to_string(task->id) +
            " attempt " + std::to_string(attempt+1) +
            " finished in " + std::to_string(ms) + " ms, exit=" +
            std::to_string(r.exit_code) +
            (r.timed_out ? " (timeout)" : ""));

            final_exit_code = r.exit_code;
            final_out       = std::move(r.output);
            final_err       = std::move(r.error);
            final_timeout   = r.timed_out;

            if (final_timeout) {
                // 超时一般不重试，直接 break
                break;
            }
    
            if (final_exit_code == 0) {
                // 成功，不重试
                break;
            }
             // 失败 + 有重试机会
            attempt++;
            task->retry_count = attempt;
            tm.updateTask(*task);
            broadcast_task_event("task_updated", *task);

            if (attempt > max_retries) {
                break;
            }

            // 指数退避：1s, 2s, 4s...
            int backoff_sec = 1 << (attempt - 1);
            Logger::info("Task " + std::to_string(task->id) +
                        " will retry after " + std::to_string(backoff_sec) + " sec");
            std::this_thread::sleep_for(std::chrono::seconds(backoff_sec));
        }

        task->exit_code   = final_exit_code;
        task->last_output = std::move(final_out);
        task->last_error  = std::move(final_err);

        if (final_canceled || task->cancel_flag) {
            task->status = TaskStatus::Canceled;
        } else if (final_timeout) {
            task->status = TaskStatus::Timeout;
        } else if (final_exit_code == 0) {
            task->status = TaskStatus::Success;
        } else {
            task->status = TaskStatus::Failed;
        }

        task->update_time = utils::now_string();
        tm.updateTask(*task);
        broadcast_task_event("task_updated", *task);

        Logger::info("Worker " + std::to_string(worker_id)
                    + " finished task " + std::to_string(task->id)
                    + ", status="
                    + std::to_string(static_cast<int>(task->status))
                    + ", exit=" + std::to_string(task->exit_code));
        m_total_finished++;
        m_current_running--;
    }
}
