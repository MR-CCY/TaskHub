#pragma once
#include <vector>
#include <optional>
#include "task.h" 


namespace taskhub {

class TaskManager {
public:
    static TaskManager& instance();

    Task::IdType add_task(const std::string& name,
                          int type,
                          const nlohmann::json& params);

    std::vector<Task> list_tasks() const;

    std::optional<Task> get_task(Task::IdType id) const;

private:
    TaskManager() = default;
    TaskManager(const TaskManager&) = delete;
    TaskManager& operator=(const TaskManager&) = delete;

private:
    Task::IdType          m_next_id{1};
    std::vector<Task>     m_tasks;
};

} // namespace taskhub