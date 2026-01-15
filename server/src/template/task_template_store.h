#pragma once
#include "json.hpp"
#include "template_types.h"
#include <optional>
#include <sqlite3.h>
#include <string>
#include <vector>

namespace taskhub::tpl {

using json = nlohmann::json;

// Task template record structure
struct TaskTemplateRecord {
  std::string templateId;
  std::string name;
  std::string description;
  std::string execType; // "Shell", "Http", "Local"
  std::string execCommand;
  json execParams;    // Default parameters
  ParamSchema schema; // Parameter schema
  int timeoutMs = 0;
  int retryCount = 0;
  int retryDelayMs = 1000;
  bool retryExpBackoff = true;
  int priority = 0;
  std::string queue = "default";
  bool captureOutput = true;
  json metadata;
  int64_t createdTsMs = 0;
  int64_t updatedTsMs = 0;

  json to_json() const;
  static TaskTemplateRecord from_json(const json &j);
};

// Store for task_template table
class TaskTemplateStore {
public:
  explicit TaskTemplateStore(sqlite3 *db);

  bool insert(const TaskTemplateRecord &tpl);
  std::optional<TaskTemplateRecord> get(const std::string &templateId);
  std::vector<TaskTemplateRecord> list();
  std::vector<TaskTemplateRecord> listByType(const std::string &execType);
  bool remove(const std::string &templateId);

private:
  sqlite3 *db_;
};

} // namespace taskhub::tpl
