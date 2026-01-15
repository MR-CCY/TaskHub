#include "task_template_handler.h"
#include "db/db.h"
#include "json.hpp"
#include "log/logger.h"
#include "template/task_template_store.h"

using json = nlohmann::json;
using namespace taskhub::tpl;

namespace taskhub {

void TaskTemplateHandler::setup_routes(httplib::Server &server) {
  server.Get("/api/task_templates",
             [](const httplib::Request &req, httplib::Response &res) {
               list(req, res);
             });

  server.Get("/api/task_templates/:id",
             [](const httplib::Request &req, httplib::Response &res) {
               get(req, res);
             });

  server.Post("/api/task_templates",
              [](const httplib::Request &req, httplib::Response &res) {
                create(req, res);
              });

  server.Delete("/api/task_templates/:id",
                [](const httplib::Request &req, httplib::Response &res) {
                  delete_(req, res);
                });
}

void TaskTemplateHandler::create(const httplib::Request &req,
                                 httplib::Response &res) {
  auto *db = Db::instance().handle();
  if (!db) {
    json response = {{"code", 500},
                     {"message", "Database not initialized"},
                     {"data", nullptr}};
    res.set_content(response.dump(), "application/json");
    return;
  }
  try {
    json body = json::parse(req.body);

    TaskTemplateRecord tpl = TaskTemplateRecord::from_json(body);

    if (tpl.templateId.empty()) {
      json response = {{"code", 400},
                       {"message", "template_id is required"},
                       {"data", nullptr}};
      res.set_content(response.dump(), "application/json");
      return;
    }

    TaskTemplateStore store(db);
    bool success = store.insert(tpl);

    if (success) {
      json response = {
          {"code", 0},
          {"message", "ok"},
          {"data", {{"ok", true}, {"template_id", tpl.templateId}}}};
      res.set_content(response.dump(), "application/json");
    } else {
      json response = {{"code", 500},
                       {"message", "Failed to create task template"},
                       {"data", nullptr}};
      res.set_content(response.dump(), "application/json");
    }
  } catch (const std::exception &e) {
    Logger::error("TaskTemplateHandler::create exception: " +
                  std::string(e.what()));
    json response = {{"code", 400},
                     {"message", std::string("Invalid request: ") + e.what()},
                     {"data", nullptr}};
    res.set_content(response.dump(), "application/json");
  }
}

void TaskTemplateHandler::get(const httplib::Request &req,
                              httplib::Response &res) {
  auto *db = Db::instance().handle();
  if (!db) {
    json response = {{"code", 500},
                     {"message", "Database not initialized"},
                     {"data", nullptr}};
    res.set_content(response.dump(), "application/json");
    return;
  }
  std::string templateId = req.path_params.at("id");

  TaskTemplateStore store(db);
  auto tpl = store.get(templateId);

  if (tpl) {
    json response = {{"code", 0},
                     {"message", "ok"},
                     {"data", {{"ok", true}, {"template", tpl->to_json()}}}};
    res.set_content(response.dump(), "application/json");
  } else {
    json response = {{"code", 404},
                     {"message", "Task template not found"},
                     {"data", nullptr}};
    res.set_content(response.dump(), "application/json");
  }
}

void TaskTemplateHandler::list(const httplib::Request &req,
                               httplib::Response &res) {
  auto *db = Db::instance().handle();
  if (!db) {
    json response = {{"code", 500},
                     {"message", "Database not initialized"},
                     {"data", nullptr}};
    res.set_content(response.dump(), "application/json");
    return;
  }
  TaskTemplateStore store(db);

  // Check for type filter
  std::string typeFilter;
  if (req.has_param("type")) {
    typeFilter = req.get_param_value("type");
  }

  std::vector<TaskTemplateRecord> templates;
  if (typeFilter.empty()) {
    templates = store.list();
  } else {
    templates = store.listByType(typeFilter);
  }

  json templateArray = json::array();
  for (const auto &tpl : templates) {
    templateArray.push_back(tpl.to_json());
  }

  json response = {{"code", 0},
                   {"message", "ok"},
                   {"data", {{"ok", true}, {"templates", templateArray}}}};
  res.set_content(response.dump(), "application/json");
}

void TaskTemplateHandler::delete_(const httplib::Request &req,
                                  httplib::Response &res) {
  auto *db = Db::instance().handle();
  if (!db) {
    json response = {{"code", 500},
                     {"message", "Database not initialized"},
                     {"data", nullptr}};
    res.set_content(response.dump(), "application/json");
    return;
  }
  std::string templateId = req.path_params.at("id");

  TaskTemplateStore store(db);
  bool success = store.remove(templateId);

  if (success) {
    json response = {{"code", 0}, {"message", "ok"}, {"data", {{"ok", true}}}};
    res.set_content(response.dump(), "application/json");
  } else {
    json response = {{"code", 500},
                     {"message", "Failed to delete task template"},
                     {"data", nullptr}};
    res.set_content(response.dump(), "application/json");
  }
}

} // namespace taskhub
