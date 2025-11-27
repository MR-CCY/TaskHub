#pragma once
#include <vector>
#include <optional>
#include "task.h" 
#include <mutex>

namespace taskhub {

class TaskManager {
public:
    static TaskManager& instance();

    Task::IdType add_task(const std::string& name,
                          int type,
                          const nlohmann::json& params);

    std::vector<Task> list_tasks() const;
    void load_from_db();
    std::optional<Task> get_task(Task::IdType id) const;
    bool set_running(Task::IdType id);
    bool set_finished(Task::IdType id, bool success,
                    int exit_code,
                    const std::string& output,
                    const std::string& error_msg);
                    
private:
    TaskManager(){
        load_from_db();
    };
    TaskManager(const TaskManager&) = delete;
    TaskManager& operator=(const TaskManager&) = delete;

private:
    mutable std::mutex m_mutex;          // ★ 保护 m_tasks
    Task::IdType          m_next_id{1};
    std::vector<Task>     m_tasks;
};

} // namespace taskhub