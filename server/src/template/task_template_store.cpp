#include "task_template_store.h"
#include "log/logger.h"
#include "utils/utils.h"
#include <cstring>

namespace taskhub::tpl {

json TaskTemplateRecord::to_json() const {
  json j;
  j["template_id"] = templateId;
  j["name"] = name;
  j["description"] = description;
  j["exec_type"] = execType;
  j["exec_command"] = execCommand;
  j["exec_params"] = execParams;
  j["schema"] = param_schema_to_json(schema);
  j["timeout_ms"] = timeoutMs;
  j["retry_count"] = retryCount;
  j["retry_delay_ms"] = retryDelayMs;
  j["retry_exp_backoff"] = retryExpBackoff;
  j["priority"] = priority;
  j["queue"] = queue;
  j["capture_output"] = captureOutput;
  j["metadata"] = metadata;
  j["created_ts_ms"] = createdTsMs;
  j["updated_ts_ms"] = updatedTsMs;
  return j;
}

TaskTemplateRecord TaskTemplateRecord::from_json(const json &j) {
  TaskTemplateRecord rec;
  rec.templateId = j.value("template_id", "");
  rec.name = j.value("name", "");
  rec.description = j.value("description", "");
  rec.execType = j.value("exec_type", "Local");
  rec.execCommand = j.value("exec_command", "");
  rec.execParams = j.value("exec_params", json::object());

  if (j.contains("schema")) {
    rec.schema = make_param_schema(j["schema"]);
  }

  rec.timeoutMs = j.value("timeout_ms", 0);
  rec.retryCount = j.value("retry_count", 0);
  rec.retryDelayMs = j.value("retry_delay_ms", 1000);
  rec.retryExpBackoff = j.value("retry_exp_backoff", true);
  rec.priority = j.value("priority", 0);
  rec.queue = j.value("queue", "default");
  rec.captureOutput = j.value("capture_output", true);
  rec.metadata = j.value("metadata", json::object());

  return rec;
}

TaskTemplateStore::TaskTemplateStore(sqlite3 *db) : db_(db) {}

bool TaskTemplateStore::insert(const TaskTemplateRecord &tpl) {
  const char *sql = "INSERT INTO task_template "
                    "(template_id, name, description, exec_type, exec_command, "
                    "exec_params_json, "
                    "schema_json, timeout_ms, retry_count, retry_delay_ms, "
                    "retry_exp_backoff, "
                    "priority, queue, capture_output, metadata_json, "
                    "created_ts_ms, updated_ts_ms) "
                    "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

  sqlite3_stmt *stmt = nullptr;
  int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    Logger::error("TaskTemplateStore::insert prepare failed: " +
                  std::string(sqlite3_errmsg(db_)));
    return false;
  }

  int64_t now = utils::now_millis();
  std::string execParamsStr = tpl.execParams.dump();
  std::string schemaStr = param_schema_to_json(tpl.schema).dump();
  std::string metadataStr = tpl.metadata.dump();

  sqlite3_bind_text(stmt, 1, tpl.templateId.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, tpl.name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, tpl.description.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, tpl.execType.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, tpl.execCommand.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 6, execParamsStr.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 7, schemaStr.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 8, tpl.timeoutMs);
  sqlite3_bind_int(stmt, 9, tpl.retryCount);
  sqlite3_bind_int(stmt, 10, tpl.retryDelayMs);
  sqlite3_bind_int(stmt, 11, tpl.retryExpBackoff ? 1 : 0);
  sqlite3_bind_int(stmt, 12, tpl.priority);
  sqlite3_bind_text(stmt, 13, tpl.queue.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 14, tpl.captureOutput ? 1 : 0);
  sqlite3_bind_text(stmt, 15, metadataStr.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 16, now);
  sqlite3_bind_int64(stmt, 17, now);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE) {
    Logger::error("TaskTemplateStore::insert step failed: " +
                  std::string(sqlite3_errmsg(db_)));
    return false;
  }

  return true;
}

std::optional<TaskTemplateRecord>
TaskTemplateStore::get(const std::string &templateId) {
  const char *sql =
      "SELECT name, description, exec_type, exec_command, exec_params_json, "
      "schema_json, "
      "timeout_ms, retry_count, retry_delay_ms, retry_exp_backoff, priority, "
      "queue, "
      "capture_output, metadata_json, created_ts_ms, updated_ts_ms "
      "FROM task_template WHERE template_id = ?;";

  sqlite3_stmt *stmt = nullptr;
  int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    Logger::error("TaskTemplateStore::get prepare failed: " +
                  std::string(sqlite3_errmsg(db_)));
    return std::nullopt;
  }

  sqlite3_bind_text(stmt, 1, templateId.c_str(), -1, SQLITE_TRANSIENT);

  std::optional<TaskTemplateRecord> result;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    TaskTemplateRecord rec;
    rec.templateId = templateId;
    rec.name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    rec.description =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    rec.execType = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    rec.execCommand =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));

    const char *execParamsStr =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
    if (execParamsStr) {
      rec.execParams = json::parse(execParamsStr);
    }

    const char *schemaStr =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
    if (schemaStr) {
      rec.schema = make_param_schema(json::parse(schemaStr));
    }

    rec.timeoutMs = sqlite3_column_int(stmt, 6);
    rec.retryCount = sqlite3_column_int(stmt, 7);
    rec.retryDelayMs = sqlite3_column_int(stmt, 8);
    rec.retryExpBackoff = sqlite3_column_int(stmt, 9) != 0;
    rec.priority = sqlite3_column_int(stmt, 10);
    rec.queue = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 11));
    rec.captureOutput = sqlite3_column_int(stmt, 12) != 0;

    const char *metadataStr =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 13));
    if (metadataStr) {
      rec.metadata = json::parse(metadataStr);
    }

    rec.createdTsMs = sqlite3_column_int64(stmt, 14);
    rec.updatedTsMs = sqlite3_column_int64(stmt, 15);

    result = rec;
  }

  sqlite3_finalize(stmt);
  return result;
}

std::vector<TaskTemplateRecord> TaskTemplateStore::list() {
  const char *sql =
      "SELECT template_id, name, description, exec_type, exec_command, "
      "exec_params_json, "
      "schema_json, timeout_ms, retry_count, retry_delay_ms, "
      "retry_exp_backoff, priority, "
      "queue, capture_output, metadata_json, created_ts_ms, updated_ts_ms "
      "FROM task_template ORDER BY exec_type, name;";

  sqlite3_stmt *stmt = nullptr;
  int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    Logger::error("TaskTemplateStore::list prepare failed: " +
                  std::string(sqlite3_errmsg(db_)));
    return {};
  }

  std::vector<TaskTemplateRecord> results;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    TaskTemplateRecord rec;
    rec.templateId =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    rec.name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    rec.description =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    rec.execType = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
    rec.execCommand =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));

    const char *execParamsStr =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
    if (execParamsStr) {
      rec.execParams = json::parse(execParamsStr);
    }

    const char *schemaStr =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
    if (schemaStr) {
      rec.schema = make_param_schema(json::parse(schemaStr));
    }

    rec.timeoutMs = sqlite3_column_int(stmt, 7);
    rec.retryCount = sqlite3_column_int(stmt, 8);
    rec.retryDelayMs = sqlite3_column_int(stmt, 9);
    rec.retryExpBackoff = sqlite3_column_int(stmt, 10) != 0;
    rec.priority = sqlite3_column_int(stmt, 11);
    rec.queue = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 12));
    rec.captureOutput = sqlite3_column_int(stmt, 13) != 0;

    const char *metadataStr =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 14));
    if (metadataStr) {
      rec.metadata = json::parse(metadataStr);
    }

    rec.createdTsMs = sqlite3_column_int64(stmt, 15);
    rec.updatedTsMs = sqlite3_column_int64(stmt, 16);

    results.push_back(rec);
  }

  sqlite3_finalize(stmt);
  return results;
}

std::vector<TaskTemplateRecord>
TaskTemplateStore::listByType(const std::string &execType) {
  const char *sql =
      "SELECT template_id, name, description, exec_type, exec_command, "
      "exec_params_json, "
      "schema_json, timeout_ms, retry_count, retry_delay_ms, "
      "retry_exp_backoff, priority, "
      "queue, capture_output, metadata_json, created_ts_ms, updated_ts_ms "
      "FROM task_template WHERE exec_type = ? ORDER BY name;";

  sqlite3_stmt *stmt = nullptr;
  int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    Logger::error("TaskTemplateStore::listByType prepare failed: " +
                  std::string(sqlite3_errmsg(db_)));
    return {};
  }

  sqlite3_bind_text(stmt, 1, execType.c_str(), -1, SQLITE_TRANSIENT);

  std::vector<TaskTemplateRecord> results;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    TaskTemplateRecord rec;
    rec.templateId =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    rec.name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    rec.description =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    rec.execType = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
    rec.execCommand =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));

    const char *execParamsStr =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
    if (execParamsStr) {
      rec.execParams = json::parse(execParamsStr);
    }

    const char *schemaStr =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
    if (schemaStr) {
      rec.schema = make_param_schema(json::parse(schemaStr));
    }

    rec.timeoutMs = sqlite3_column_int(stmt, 7);
    rec.retryCount = sqlite3_column_int(stmt, 8);
    rec.retryDelayMs = sqlite3_column_int(stmt, 9);
    rec.retryExpBackoff = sqlite3_column_int(stmt, 10) != 0;
    rec.priority = sqlite3_column_int(stmt, 11);
    rec.queue = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 12));
    rec.captureOutput = sqlite3_column_int(stmt, 13) != 0;

    const char *metadataStr =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 14));
    if (metadataStr) {
      rec.metadata = json::parse(metadataStr);
    }

    rec.createdTsMs = sqlite3_column_int64(stmt, 15);
    rec.updatedTsMs = sqlite3_column_int64(stmt, 16);

    results.push_back(rec);
  }

  sqlite3_finalize(stmt);
  return results;
}

bool TaskTemplateStore::remove(const std::string &templateId) {
  const char *sql = "DELETE FROM task_template WHERE template_id = ?;";

  sqlite3_stmt *stmt = nullptr;
  int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    Logger::error("TaskTemplateStore::remove prepare failed: " +
                  std::string(sqlite3_errmsg(db_)));
    return false;
  }

  sqlite3_bind_text(stmt, 1, templateId.c_str(), -1, SQLITE_TRANSIENT);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE) {
    Logger::error("TaskTemplateStore::remove step failed: " +
                  std::string(sqlite3_errmsg(db_)));
    return false;
  }

  return true;
}

} // namespace taskhub::tpl
