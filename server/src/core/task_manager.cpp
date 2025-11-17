#include "task_manager.h"
namespace taskhub {
    TaskManager &TaskManager::instance()
    {
        static TaskManager instance;
        return instance;
    }

    Task::IdType taskhub::TaskManager::add_task(const std::string &name, int type, const nlohmann::json &params)
    {
        Task task;
        task.id = m_next_id++;
        task.name = name;
        task.type = type;
        task.params = params;
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
} // namespace taskhub
