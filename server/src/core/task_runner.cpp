#include "task_runner.h"
#include "core/logger.h"
#include "task_manager.h"
#include <array>
#include <cstdio>
#include "core/ws_task_events.h"
#include "core/http_response.h"
namespace taskhub {

static std::pair<int, std::string> run_command(const std::string& cmd) {
    std::array<char, 256> buffer{};
    std::string result;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return { -1, "popen() failed" };
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    int rc = pclose(pipe);  // rc 一般是进程返回码 << 8，不用太纠结

    return { rc, result };
}


    TaskRunner &taskhub::TaskRunner::instance()
    {
        static TaskRunner instance;
        return instance;
        // TODO: 在此处插入 return 语句
    }

    void taskhub::TaskRunner::start()
    {
        if (m_started) return;
        m_started = true;
        m_thread = std::thread(&TaskRunner::worker_loop, this);
    }

    void taskhub::TaskRunner::stop()
    {
       
    }

    void taskhub::TaskRunner::enqueue(Task::IdType id)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push(id);
        }
        m_cv.notify_one();
    }

    void taskhub::TaskRunner::worker_loop()
    {
        while (true) {
            Task::IdType task_id;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this]() {
                    return m_stopping || !m_queue.empty();
                });
                if (m_stopping && m_queue.empty()) {
                    break;
                }
                task_id = m_queue.front();
                m_queue.pop();
            }
            // 处理任务
            Logger::info("Processing task id: " + std::to_string(task_id));
              // 这里开始处理任务 id
            // 1. 标记为 running
            TaskManager::instance().set_running(task_id);
            // ★ 广播任务更新事件
            taskhub::broadcast_task_event("task_updated", TaskManager::instance().get_task(task_id).value()); 
            // 2. 取任务，提取 cmd
            auto opt = TaskManager::instance().get_task(task_id);
            if (!opt) {
                // theoretically shouldn't happen
                continue;
            }
            auto task = *opt;

            std::string cmd;
            if (task.params.contains("cmd") && task.params["cmd"].is_string()) {
                cmd = task.params["cmd"].get<std::string>();
            } else {
                TaskManager::instance().set_finished(task_id, false, -1, "", "missing cmd in params");
                continue;
            }
            Logger::info("Task " + std::to_string(task_id) + " start: " + cmd);
            // 3. 执行命令
            auto [exit_code, output] = run_command(cmd);
            
            Logger::info("Task " + std::to_string(task_id) + " finished, exit=" + std::to_string(exit_code));
            bool success = (exit_code == 0);
            TaskManager::instance().set_finished( task_id, success, exit_code, output, success ? "" : "command failed");
            // ★ 广播任务更新事件
            taskhub::broadcast_task_event("task_updated", TaskManager::instance().get_task(task_id).value());
        }

    }
}