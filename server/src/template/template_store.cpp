#include "template_store.h"
#include <mutex>  
#include <unordered_set>
#include "log/logger.h"
#include "json.hpp"
#include "utils/utils.h"
using json = nlohmann::json;
namespace taskhub::tpl {
    TemplateStore &TemplateStore::instance()
    {
        static TemplateStore instance;
        return instance;
    }
    // ... existing code ...
        /**
         * @brief 创建一个新的任务模板并存储到数据库中
         * @param t 要创建的任务模板对象
         * @return 创建成功返回true，否则返回false
         */
        bool TemplateStore::create(const TaskTemplate &t)
        {
            sqlite3* db = Db::instance().handle();
            const char* sql = "INSERT INTO task_templates (template_id, name, description, task_json_template, schema_json,created_at_ms,updated_at_ms) VALUES (?,?,?,?,?,?,?);";
            sqlite3_stmt* stmt = nullptr;
            bool ok = false;
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                Logger::error("INSERT template prepare failed: " + Db::instance().last_error());
            } else {
                // 绑定基本字段信息
                sqlite3_bind_text(stmt, 1, t.templateId.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 2, t.name.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 3, t.description.c_str(), -1    , SQLITE_TRANSIENT);
                std::string task_json_str = t.taskJsonTemplate.dump();
                sqlite3_bind_text(stmt, 4, task_json_str.c_str(), -1,   SQLITE_TRANSIENT);
                json schema_json = param_schema_to_json(t.schema);
                std::string schema_json_str = schema_json.dump();
                Logger::debug("INSERT template schema_json: " + schema_json_str);
                sqlite3_bind_text(stmt, 5, schema_json_str.c_str(), -1, SQLITE_TRANSIENT);
                
                // 设置创建和更新时间戳
                const auto current_time_ms = utils::now_millis();
                sqlite3_bind_int64(stmt, 6, static_cast<sqlite3_int64>(current_time_ms));
                sqlite3_bind_int64(stmt, 7, static_cast<sqlite3_int64>(current_time_ms));
                ok = sqlite3_step(stmt) == SQLITE_DONE;
                if (!ok) {
                    Logger::error("INSERT template step failed: " + Db::instance().last_error());
                }
            }
            if (stmt) {
                sqlite3_finalize(stmt);
            }
    
            // 如果插入成功，将模板添加到内存映射中
            if (ok) {
                std::lock_guard<std::shared_mutex> lock(_mu);
                _map.insert(std::make_pair(t.templateId, t));
            }
    
            return ok;
        }
    // ... existing code ...
        /**
         * 根据模板ID获取任务模板
         * 
         * 首先尝试从内存缓存中查找模板，如果未找到则从数据库中查询。
         * 查询到的结果会被缓存到内存中以提高后续访问速度。
         * 
         * @param templateId 要获取的模板的唯一标识符
         * @return 如果找到模板则返回包含模板数据的std::optional<TaskTemplate>，否则返回std::nullopt
         */
        std::optional<TaskTemplate> TemplateStore::get(const std::string &templateId)
        {
            {
                std::lock_guard<std::shared_mutex> lock(_mu);  // 添加锁保护
                auto it = _map.find(templateId);
                if (it != _map.end()) {
                    return it->second;
                }
            }
    
            sqlite3* db = Db::instance().handle();
            const char* sql = "SELECT name, description, task_json_template, schema_json FROM task_templates WHERE template_id = ?;";
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                Logger::error("SELECT template prepare failed: " + Db::instance().last_error());
                return std::nullopt;
            }
            sqlite3_bind_text(stmt, 1, templateId.c_str(), -1, SQLITE_TRANSIENT);
            TaskTemplate t;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                t.templateId = templateId;
                t.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                t.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                const char* task_json_cstr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                if (task_json_cstr) {
                    t.taskJsonTemplate = json::parse(task_json_cstr);
                } else {
                    t.taskJsonTemplate = json::object();
                }
                const char* schema_json_cstr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                if (schema_json_cstr) {
                    json schema_json = json::parse(schema_json_cstr);
                    // 解析参数定义
                    if (schema_json.contains("params") && schema_json["params"].is_array()) {
                        for (const auto& param_json : schema_json["params"]) {      
                            tpl::ParamDef param;
                            param.name = param_json.value("name", "");
                            param.type = StringToParamType(param_json.value("type", ""));
                            param.required = param_json.value("required", false);
                            param.defaultValue = param_json.value("defaultValue", json());
                            t.schema.params.push_back(param);
                        }
                    }
                } else {
                    t.schema.params.clear();
                }
                sqlite3_finalize(stmt);
                {
                    std::lock_guard<std::shared_mutex> lock(_mu);
                    _map.insert(std::make_pair(t.templateId, t));
                }
                return t;
            }
            sqlite3_finalize(stmt);
            return std::nullopt;
        }
    // ... existing code ...
        /**
         * @brief 获取所有任务模板列表
         * 
         * 从数据库和内存缓存中获取所有任务模板。首先从数据库中查询所有模板，
         * 然后与内存中的模板进行合并，去除重复项。
         * 
         * @return std::vector<TaskTemplate> 包含所有任务模板的向量
         */
        std::vector<TaskTemplate> TemplateStore::list() const
        {
            std::vector<TaskTemplate> result;
            sqlite3* db = Db::instance().handle();
            const char* sql = "SELECT template_id, name, description, task_json_template, schema_json FROM task_templates;";
            sqlite3_stmt* stmt = nullptr;
            
            // 从数据库中查询所有模板记录
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    TaskTemplate t;
                    t.templateId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    t.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                    t.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                    const char* task_json_cstr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                    t.taskJsonTemplate = task_json_cstr ? json::parse(task_json_cstr) : json::object();
    
                    // 解析模板参数定义
                    const char* schema_json_cstr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                    if (schema_json_cstr) {
                        json schema_json = json::parse(schema_json_cstr);
                        if (schema_json.contains("params") && schema_json["params"].is_array()) {
                            for (const auto& param_json : schema_json["params"]) {
                                tpl::ParamDef param;
                                param.name = param_json.value("name", "");
                                param.type = StringToParamType(param_json.value("type", ""));
                                param.required = param_json.value("required", false);
                                param.defaultValue = param_json.value("defaultValue", json());
                                t.schema.params.push_back(param);
                            }
                        }
                    }
    
                    result.push_back(std::move(t));
                }
                sqlite3_finalize(stmt);
            } else {
                Logger::error("SELECT templates prepare failed: " + Db::instance().last_error());
            }
    
            // 合并内存中的模板，避免重复
            std::unordered_set<std::string> ids;
            ids.reserve(result.size() + _map.size());
            for (const auto& item : result) {
                ids.insert(item.templateId);
            }
    
            // 添加内存缓存中独有的模板
            std::shared_lock<std::shared_mutex> lock(_mu);
            for (const auto& pair : _map) {
                if (ids.insert(pair.first).second) {
                    result.push_back(pair.second);
                }
            }
    
            return result;
        }
    // ... existing code ...
    // ... existing code ...
        /**
         * @brief 从数据库中删除指定ID的任务模板
         * 
         * @param templateId 要删除的模板ID
         * @return bool 删除成功返回true，否则返回false
         */
        bool TemplateStore::remove(const std::string &templateId)
        {
            sqlite3* db = Db::instance().handle();
            const char* sql = "DELETE FROM task_templates WHERE template_id = ?;";
            sqlite3_stmt* stmt = nullptr;
            bool ok = false;
    
            // 准备SQL语句
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                Logger::error("DELETE template prepare failed: " + Db::instance().last_error());
            } else {
                sqlite3_bind_text(stmt, 1, templateId.c_str(), -1, SQLITE_TRANSIENT);
                ok = sqlite3_step(stmt) == SQLITE_DONE;
                if (!ok) {
                    Logger::error("DELETE template step failed: " + Db::instance().last_error());
                }
                sqlite3_finalize(stmt);
            }
    
            // 从内存缓存中移除模板
            if (ok) {
                std::lock_guard<std::shared_mutex> lock(_mu);
                _map.erase(templateId);
            }
    
            return ok;
        }
    // ... existing code ...
}
