#include "task_manager.h"
#include <sstream>
#include "utils.h"
namespace taskhub {
    TaskManager &TaskManager::instance()
    {
        static TaskManager instance;
        return instance;
    }

    Task::IdType taskhub::TaskManager::add_task(const std::string &name, int type, const nlohmann::json &params)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        Task task;
        task.id = m_next_id++;
        task.name = name;
        task.type = type;
        task.params = params;
        task.create_time =  utils::now_string();
        task.update_time =  task.create_time;
    
        m_tasks.push_back(task);
        return task.id;
    }

    std::vector<Task> taskhub::TaskManager::list_tasks() const
    {
        return m_tasks;
    }

    std::optional<Task> taskhub::TaskManager::get_task(Task::IdType id) const
    {
        for (const auto &task : m_tasks) {
            if (task.id == id) {
                return task;
            }
        }
        return std::optional<Task>();
    }
    bool TaskManager::set_running(Task::IdType id)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& t : m_tasks) {
            if (t.id == id) {
                t.status = TaskStatus::Running;
                t.update_time = utils::now_string();
                return true;
            }
        }
        return false;
    }
    bool TaskManager::set_finished(Task::IdType id, bool success, int exit_code, const std::string &output, const std::string &error_msg)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& t : m_tasks) {
            if (t.id == id) {
                t.status     = success ? TaskStatus::Success : TaskStatus::Failed;
                t.exit_code  = exit_code;
                t.last_output = output;
                t.last_error  = error_msg;
                t.update_time = utils::now_string();
                return true;
            }
        }
        return false;
    }
} // namespace taskhub
