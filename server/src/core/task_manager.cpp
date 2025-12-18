#include "task_manager.h"
#include <sstream>
#include <memory>
#include "utils/utils.h"
#include "db/db.h"
#include "log/logger.h"
namespace taskhub {
    TaskManager &TaskManager::instance()
    {
        static TaskManager instance;
        return instance;
    }

    Task::IdType taskhub::TaskManager::add_task(const std::string &name, int type, const nlohmann::json &params)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        TaskPtr task = std::make_shared<Task>();
        task->id = m_next_id++;
        task->name = name;
        task->type = type;
        task->params = params;
        task->create_time =  utils::now_string();
        task->update_time =  task->create_time;
        task->max_retries=params.value("max_retries", 0);
        task->timeout_sec=params.value("timeout_sec",0);

        // 写入 DB
        sqlite3* db = Db::instance().handle();
        const char* sql = "INSERT INTO tasks (id, name, type, status, params, create_time, update_time, exit_code, last_output, last_error, max_retries, retry_count, timeout_sec, cancel_flag) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            Logger::error("INSERT tasks prepare failed: " + Db::instance().last_error());
        } else {
            sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(task->id));
            sqlite3_bind_text(stmt,  2, task->name.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt,   3, task->type);
            std::string status_str =  Task::status_to_string(task->status);
            sqlite3_bind_text(stmt,  4, status_str.c_str(), -1, SQLITE_TRANSIENT);
            std::string params_str = task->params.dump();
            sqlite3_bind_text(stmt,  5, params_str.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt,  6, task->create_time.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt,  7, task->update_time.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt,   8, task->exit_code);
            sqlite3_bind_text(stmt,  9, task->last_output.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 10, task->last_error.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt,  11, task->max_retries);
            sqlite3_bind_int(stmt,  12, task->retry_count);
            sqlite3_bind_int(stmt,  13, task->timeout_sec);
            sqlite3_bind_int(stmt,  14, task->cancel_flag ? 1 : 0);

            if (sqlite3_step(stmt) != SQLITE_DONE) {
                Logger::error("INSERT tasks step failed: " + Db::instance().last_error());
            }
            sqlite3_finalize(stmt);
        }

        m_tasks.push_back(task);
        return task->id;
    }

    std::vector<Task> taskhub::TaskManager::list_tasks() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<Task> res;
        res.reserve(m_tasks.size());
        for (const auto& t : m_tasks) {
            if (t) res.push_back(*t);
        }
        return res;
    }

    void TaskManager::load_from_db()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        sqlite3* db = Db::instance().handle();
        const char* sql = "SELECT id, name, type, status, params, create_time, update_time, exit_code, last_output, last_error, max_retries, retry_count, timeout_sec, cancel_flag FROM tasks ORDER BY id;";
    
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            Logger::error("prepare SELECT tasks failed: " + Db::instance().last_error());
            return;
        }
        Task::IdType max_id = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            Task t;
            t.id = sqlite3_column_int64(stmt, 0);
            t.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            t.type = sqlite3_column_int64(stmt, 2);
            std::string statusStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            t.status = Task::string_to_status(statusStr);
            const unsigned char* paramsText = sqlite3_column_text(stmt, 4);
            if (paramsText) {
                try {
                    t.params = nlohmann::json::parse(reinterpret_cast<const char*>(paramsText));
                } catch (...) {
                    t.params = nlohmann::json::object();
                }
            }
            t.create_time = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            t.update_time = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            t.exit_code   = sqlite3_column_int(stmt, 7);

            const unsigned char* outText = sqlite3_column_text(stmt, 8);
            if (outText)
                t.last_output = reinterpret_cast<const char*>(outText);

            const unsigned char* errText = sqlite3_column_text(stmt, 9);
            if (errText)
                t.last_error = reinterpret_cast<const char*>(errText);
            t.max_retries = sqlite3_column_int(stmt, 10);
            t.retry_count = sqlite3_column_int(stmt, 11);
            t.timeout_sec = sqlite3_column_int(stmt, 12);
            t.cancel_flag = sqlite3_column_int(stmt, 13) != 0;
    
            TaskPtr ptr = std::make_shared<Task>(std::move(t));
            if (ptr->id > max_id) max_id = ptr->id;
            m_tasks.push_back(std::move(ptr));

        }

        sqlite3_finalize(stmt);

        m_next_id = max_id + 1;
        Logger::info("TaskManager loaded " + std::to_string(m_tasks.size()) + " tasks from DB");

    }

    std::optional<Task> taskhub::TaskManager::get_task(Task::IdType id) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto &task : m_tasks) {
            if (task && task->id == id) {
                return *task;
            }
        }
        return std::optional<Task>();
    }
    TaskPtr TaskManager::get_task_ptr(Task::IdType id) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto &task : m_tasks) {
            if (task && task->id == id) {
                return task;
            }
        }
        return nullptr;
    }
    bool TaskManager::set_running(Task::IdType id)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& t : m_tasks) {
            if (t && t->id == id) {
                t->status = TaskStatus::Running;
                t->update_time = utils::now_string();

                sqlite3* db = Db::instance().handle();
                const char* sql = "UPDATE tasks SET status = ?, update_time = ? WHERE id = ?;";

                sqlite3_stmt* stmt = nullptr;
                if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                    Logger::error("UPDATE tasks(running) prepare failed: " + Db::instance().last_error());
                    return false;
                }
            
                std::string status_str = Task::status_to_string(t->status);
                sqlite3_bind_text(stmt, 1, status_str.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 2, t->update_time.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(t->id));
            
                bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
                if (!ok) {
                    Logger::error("UPDATE tasks(running) step failed: " + Db::instance().last_error());
                }
                sqlite3_finalize(stmt);
                return true;
            }
        }
        return false;
    }
    bool TaskManager::updateTask(const Task& updated)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        TaskPtr target;
        for (auto& t : m_tasks) {
            if (t && t->id == updated.id) {
                target = t;
                break;
            }
        }
        if (!target) {
            Logger::warn("updateTask: Task id " + std::to_string(updated.id) + " not found");
            return false;
        }

        // 同步内存中的任务信息
        *target = updated;

        sqlite3* db = Db::instance().handle();
        const char* sql = R"(
            UPDATE tasks
            SET name = ?, type = ?, status = ?, params = ?, create_time = ?, update_time = ?, exit_code = ?, last_output = ?, last_error = ?, max_retries = ?, retry_count = ?, timeout_sec = ?, cancel_flag = ?
            WHERE id = ?;
        )";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            Logger::error("UPDATE tasks prepare failed: " + Db::instance().last_error());
            return false;
        }

        std::string status_str = Task::status_to_string(target->status);
        std::string params_str = target->params.dump();

        sqlite3_bind_text(stmt, 1, target->name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, target->type);
        sqlite3_bind_text(stmt, 3, status_str.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, params_str.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, target->create_time.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 6, target->update_time.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 7, target->exit_code);
        sqlite3_bind_text(stmt, 8, target->last_output.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 9, target->last_error.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 10, target->max_retries);
        sqlite3_bind_int(stmt, 11, target->retry_count);
        sqlite3_bind_int(stmt, 12, target->timeout_sec);
        sqlite3_bind_int(stmt, 13, target->cancel_flag ? 1 : 0);
        sqlite3_bind_int64(stmt, 14, static_cast<sqlite3_int64>(target->id));

        bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
        if (!ok) {
            Logger::error("UPDATE tasks step failed: " + Db::instance().last_error());
        }
        sqlite3_finalize(stmt);
        return ok;
    }
    bool TaskManager::set_finished(Task::IdType id, bool success, int exit_code, const std::string &output, const std::string &error_msg)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& t : m_tasks) {
            if (t && t->id == id) {
                t->status     = success ? TaskStatus::Success : TaskStatus::Failed;
                t->exit_code  = exit_code;
                t->last_output = output;
                t->last_error  = error_msg;
                t->update_time = utils::now_string();

                sqlite3* db = Db::instance().handle();
                const char* sql = R"(
                    UPDATE tasks
                    SET status = ?, update_time = ?, exit_code = ?, last_output = ?, last_error = ?
                    WHERE id = ?;
                )";
            
                sqlite3_stmt* stmt = nullptr;
                if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                    Logger::error("UPDATE tasks(finished) prepare failed: " + Db::instance().last_error());
                    return false;
                }
            
                std::string status_str = Task::status_to_string(t->status);
                sqlite3_bind_text(stmt, 1, status_str.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 2, t->update_time.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(stmt,   3, t->exit_code);
                sqlite3_bind_text(stmt,  4, t->last_output.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt,  5, t->last_error.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int64(stmt, 6, static_cast<sqlite3_int64>(t->id));
            
                bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
                if (!ok) {
                    Logger::error("UPDATE tasks(finished) step failed: " + Db::instance().last_error());
                }
                sqlite3_finalize(stmt);

                return true;
            }
        }
        return false;
    }
} // namespace taskhub
