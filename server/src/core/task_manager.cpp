#include "task_manager.h"
#include <sstream>
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
        auto now = std::chrono::system_clock::now();
        auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::ostringstream time_stream;
        time_stream << std::put_time(std::localtime(&tt), "%Y-%m-%d %H:%M:%S")
                    << '.' << std::setw(3) << std::setfill('0') << ms.count();
        task.create_time =  time_stream.str();
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
} // namespace taskhub
