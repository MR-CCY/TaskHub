#include "template_handler.h"
#include "auth/auth_manager.h"
#include "httplib.h"
#include "log/logger.h"
#include "json.hpp"
#include "template/template_store.h"
#include "template/template_renderer.h"
#include "core/http_response.h"
#include "dag/dag_service.h"
#include "dag/dag_thread_pool.h"
#include "utils/utils.h"
#include <unordered_map>
using json=nlohmann::json;

namespace taskhub {
    void TemplateHandler::setup_routes(httplib::Server & server)
    {
        server.Post("/template", create);
        server.Get(R"(/template/([^/]+))", get);
        server.Get("/templates", list);
        server.Delete(R"(/template/([^/]+))", delete_);
        server.Post(R"(/template/update/([^/]+))", update);
        server.Post("/template/render", render);
        server.Post("/template/run", run);
        server.Post("/template/run_async", runAsync);
    }

    // Request: POST /template
    //   Body JSON: {"template_id":"t1","name":"echo template","description":"...","task_json_template":{...},"schema":{...}}
    // Response:
    //   200 {"ok":true,"template_id":"t1"}
    //   400 {"ok":false,"error":"bad request" | "missing required fields"}
    //   404 n/a; 500 {"ok":false,"error":"template id already exists" | "internal server error"}
    void TemplateHandler::create(const httplib::Request &req, httplib::Response &res)
    {
        Logger::info("Post /template/create");
        json req_json;
        try {
            req_json = json::parse(req.body);
        } catch (json::exception& e) {
            Logger::error(std::string("Failed to parse JSON request: ") + e.what());
            resp::error(res, 400, R"({"ok":false,"error":"bad request"})");
            return;
        }
        try {
            auto template_id = req_json.value("template_id", "");
            auto name = req_json.value("name", "");
            auto description = req_json.value("description", "");
            auto task_json_template = req_json.value("task_json_template", json::object());
            auto schema = req_json.value("schema", json::object());
            if(template_id.empty() || name.empty() || task_json_template.is_null()){
                resp::error(res, 400, R"({"ok":false,"error":"missing required fields"})");
                return;
            }
            auto& store = tpl::TemplateStore::instance();
            tpl::TaskTemplate tpl;
            tpl.templateId = template_id;
            tpl.name = name;
            tpl.description = description;
            tpl.taskJsonTemplate = task_json_template;
            tpl.schema = tpl::make_param_schema(schema);
            bool ok = store.create(tpl);
            if(!ok){
                resp::error(res, 500, "{\"ok\":false,\"error\":\"template id already exists\"}");
                return;
            }
            
            json out;
            out["ok"] = true;
            out["template_id"] = template_id;
            res.status = 200;
            res.set_content(out.dump(), "application/json");
            return;
        } catch (const std::exception& e) {
            resp::error(res, 500, R"({"ok":false,"error":"internal server error"})");
            return;
        }
    }

    // Request: GET /template/{id}
    // Response:
    //   200 {"ok":true,"data":{template json}}
    //   404 {"ok":false,"error":"template not found"}; 500 {"ok":false,"error":"internal server error"}
    void TemplateHandler::get(const httplib::Request &req, httplib::Response &res)
    {
        Logger::info("Get /template/get");
        try {
            auto template_id = req.matches[1].str();
            auto& store = tpl::TemplateStore::instance();
            auto tpl_opt = store.get(template_id);
            if (!tpl_opt) {
                resp::error(res, 404, R"({"ok":false,"error":"template not found"})");
                return;
            }
            json out;
            out["ok"] = true;
            out["data"] = tpl_opt->to_json();
            res.status = 200;
            res.set_content(out.dump(), "application/json");
            return;
        } catch (const std::exception& e) {
            Logger::error(std::string("Failed to get template: ") + e.what());
            resp::error(res, 500, R"({"ok":false,"error":"internal server error"})");
            return;
        }
    }
    // Request: GET /templates
    // Response: 200 {"ok":true,"data":[{template...}]}; 500 {"ok":false,"error":"internal server error"}
    void TemplateHandler::list(const httplib::Request &req, httplib::Response &res)
    {
        Logger::info("Get /template/list");
        try {
            auto& store = tpl::TemplateStore::instance();
            auto templates = store.list();
            json data = json::array();
            for(const auto& t : templates){
                data.push_back(t.to_json());
            }
            json out;
            out["ok"] = true;
            out["data"] = data;
            res.status = 200;
            res.set_content(out.dump(), "application/json");
            return;
        } catch (const std::exception& e) {
            Logger::error(std::string("Failed to list templates: ") + e.what());
            resp::error(res, 500, R"({"ok":false,"error":"internal server error"})");
            return;
        }
    }
    // Request: DELETE /template/{id}
    // Response:
    //   200 {"ok":true}
    //   404 {"ok":false,"error":"template not found"}; 500 {"ok":false,"error":"internal server error"}
    void TemplateHandler::delete_(const httplib::Request &req, httplib::Response &res)
    {
        Logger::info("Delete /template/delete");
        try {
            auto template_id = req.matches[1].str();
            auto& store = tpl::TemplateStore::instance();
            bool ok = store.remove(template_id);
            if (!ok) {
                resp::error(res, 404, R"({"ok":false,"error":"template not found"})");
            } else {
                json out;
                out["ok"] = true;
                res.status = 200;
                res.set_content(out.dump(), "application/json");
            }
            return;
        } catch (const std::exception& e) {
            Logger::error(std::string("Failed to delete template: ") + e.what());
            resp::error(res, 500, R"({"ok":false,"error":"internal server error"})");
            return;
        }
    }
    // Request: POST /template/update/{id}
    //   Body: 部分字段更新（name/description/task_json_template/schema）
    // Response: 未实现（TODO M13.3）
    void TemplateHandler::update(const httplib::Request &req, httplib::Response &res)
    {
        // TODO(M13.3): implement update
    }
    // Request: POST /template/render
    //   Body JSON: {"template_id":"t1","params":{...}}
    // Response:
    //   200 {"ok":true,"data":{"ok":true,"error":"","rendered":{...}}}
    //   400 {"ok":false,"error":"missing required fields" | <render error>,"data":{RenderResult}}
    //   404 {"ok":false,"error":"template not found"}; 500 {"ok":false,"error":"internal server error"}
    void TemplateHandler::render(const httplib::Request &req, httplib::Response &res)
    {
        Logger::info("Post /template/render");
        json req_json;
        try {
            req_json = json::parse(req.body);
        } catch (json::exception& e) {
            Logger::error(std::string("Failed to parse JSON request: ") + e.what());
            resp::error(res, 400, R"({"ok":false,"error":"bad request"})");
            return;
        }
        try {
            auto template_id = req_json.value("template_id", "");
            auto params = req_json.value("params", json::object());
            if (template_id.empty() || !params.is_object()) {
                resp::error(res, 400, R"({"ok":false,"error":"missing required fields"})");
                return;
            }
            auto& store = tpl::TemplateStore::instance();
            auto tpl_opt = store.get(template_id);
            if (!tpl_opt) {
                resp::error(res, 404, R"({"ok":false,"error":"template not found"})");
                return;
            }
            tpl::ParamMap p;
            for (auto& [k, v] : params.items()) {
                p[k] = v;
            }
            auto rendered = tpl::TemplateRenderer::render(*tpl_opt, p);
            if (!rendered.ok) {
                json out;
                out["ok"] = false;
                out["error"] = rendered.error;
                out["data"] = rendered.to_json();
                res.status = 400;
                res.set_content(out.dump(), "application/json");
                return;
            }
            json out;
            out["ok"] = true;
            out["data"] = rendered.to_json();
            res.status = 200;
            res.set_content(out.dump(), "application/json");
            return;
        } catch (const std::exception& e) {
            Logger::error(std::string("Failed to render template: ") + e.what());
            resp::error(res, 500, R"({"ok":false,"error":"internal server error"})");
            return;
        }
    }
    // Request: POST /template/run
    //   Body JSON: {"template_id":"t1","params":{...}}
    //   行为：渲染模板、为每个 task/task_list 注入 run_id，调用 DagService::runDag
    // Response:
    //   200 {"ok":true,"message":string,"nodes":[{"id":"a","run_id":"...","result":{status(int),message,exit_code,duration_ms,stdout,stderr,attempt,max_attempts,metadata}}],"run_id":string,"summary":{"total":N,"success":[ids],"failed":[ids],"skipped":[ids]}}
    //   400 {"ok":false,"error":"missing required fields" | <render error>,"data":{RenderResult}}
    //   404 {"ok":false,"error":"template not found"}
    //   500 {"ok":false,"error":<dag error> | "internal server error"}
    void TemplateHandler::run(const httplib::Request &req, httplib::Response &res)
    {
        Logger::info("Post /template/run");
        json req_json;
        try {
            req_json = json::parse(req.body);
        } catch (json::exception& e) {
            Logger::error(std::string("Failed to parse JSON request: ") + e.what());
            resp::error(res, 400, R"({"ok":false,"error":"bad request"})");
            return;
        }
        try {
            auto template_id = req_json.value("template_id", "");
            auto params = req_json.value("params", json::object());
            if (template_id.empty() || !params.is_object()) {
                resp::error(res, 400, R"({"ok":false,"error":"missing required fields"})");
                return;
            }
            auto& store = tpl::TemplateStore::instance();
            auto tpl_opt = store.get(template_id);
            if (!tpl_opt) {
                resp::error(res, 404, R"({"ok":false,"error":"template not found"})");
                return;
            }
            tpl::ParamMap p;
            for (auto& [k, v] : params.items()) {
                p[k] = v;
            }
            auto r = tpl::TemplateRenderer::render(*tpl_opt, p);
            if (!r.ok) {
                json out;
                out["ok"] = false;
                out["error"] = r.error;
                out["data"] = r.to_json();
                res.status = 400;
                res.set_content(out.dump(), "application/json");
                return;
            }
            const std::string runId = std::to_string(utils::now_millis()) + "_" + utils::random_string(6);
            if (r.rendered.contains("tasks") && r.rendered["tasks"].is_array()) {
                for (auto& jt : r.rendered["tasks"]) {
                    if (!jt.contains("id") || !jt["id"].is_string()) {
                        resp::error(res, 400, R"({"ok":false,"error":"task id is required"})");
                        return;
                    }
                    jt["run_id"] = runId;
                }
            } else if (r.rendered.contains("task") && r.rendered["task"].is_object()) {
                if (!r.rendered["task"].contains("id") || !r.rendered["task"]["id"].is_string()) {
                    resp::error(res, 400, R"({"ok":false,"error":"task id is required"})");
                    return;
                }
                r.rendered["task"]["run_id"] = runId;
            } else {
                if (!r.rendered.contains("id") || !r.rendered["id"].is_string()) {
                    resp::error(res, 400, R"({"ok":false,"error":"task id is required"})");
                    return;
                }
                r.rendered["run_id"] = runId;
            }

            auto dagResult= dag::DagService::instance().runDag(r.rendered, runId);
            if(!dagResult.success){
                json out;
                out["ok"] = false;
                out["error"] = dagResult.message;
                res.status = 500;
                res.set_content(out.dump(), "application/json");
                return;
            }

            std::vector<std::string> successIds;
            std::vector<std::string> failedIds;
            std::vector<std::string> skippedIds;
            for (const auto& kv : dagResult.taskResults) {
                const auto& id = kv.first;
                auto status = kv.second.status;
                const std::string idStr = id.value;
                switch (status) {
                case core::TaskStatus::Success:
                    successIds.push_back(idStr);
                    break;
                case core::TaskStatus::Failed:
                case core::TaskStatus::Timeout:
                    failedIds.push_back(idStr);
                    break;
                case core::TaskStatus::Skipped:
                    skippedIds.push_back(idStr);
                    break;
                default:
                    break;
                }
            }

            json respJson;
            respJson["ok"]      = dagResult.success;
            respJson["message"] = dagResult.message;
            respJson["nodes"]= dagResult.to_json();
            respJson["run_id"] = runId;
            json summary;
            summary["total"]   = dagResult.taskResults.size();
            summary["success"] = successIds;
            summary["failed"]  = failedIds;
            summary["skipped"] = skippedIds;
            respJson["summary"] = summary;
            res.set_content(respJson.dump(), "application/json");
        } catch (const std::exception& e) {
            Logger::error(std::string("Failed to run template: ") + e.what());
            resp::error(res, 500, std::string(R"({"ok":false,"error":")") + e.what() + R"("})");
            return;
        }
    }
    // Request: POST /template/run_async
    //   Body JSON: {"template_id":"t1","params":{...}}
    //   行为：渲染模板、为每个 task 注入 run_id，后台线程调用 DagService::runDag
    // Response:
    //   200 {"code":0,"message":"ok","data":{"run_id":string,"task_ids":[{"logical":<task logical id>,"task_id":<task id>},...]}}
    //   400/404 通过 resp::error 返回：code=400 (bad request/render error) 或 404 (template not found)，data=null
    void TemplateHandler::runAsync(const httplib::Request &req, httplib::Response &res)
    {
        Logger::info("Post /template/run_async");
        json req_json;
        try {
            req_json = json::parse(req.body);
        } catch (json::exception& e) {
            resp::error(res, 400, R"({"ok":false,"error":"bad request"})");
            return;
        }
        try {
            auto template_id = req_json.value("template_id", "");
            auto params = req_json.value("params", json::object());
            if (template_id.empty() || !params.is_object()) {
                resp::error(res, 400, R"({"ok":false,"error":"missing required fields"})");
                return;
            }
            auto& store = tpl::TemplateStore::instance();
            auto tpl_opt = store.get(template_id);
            if (!tpl_opt) {
                resp::error(res, 404, R"({"ok":false,"error":"template not found"})");
                return;
            }
            tpl::ParamMap p;
            for (auto& [k, v] : params.items()) {
                p[k] = v;
            }
            auto r = tpl::TemplateRenderer::render(*tpl_opt, p);
            if (!r.ok) {
                resp::error(res, 400, r.error);
                return;
            }

            const std::string runId = std::to_string(utils::now_millis()) + "_" + utils::random_string(6);
            std::vector<json> taskList;
            if (r.rendered.contains("tasks") && r.rendered["tasks"].is_array()) {
                for (auto& jt : r.rendered["tasks"]) {
                    if (!jt.contains("id") || !jt["id"].is_string()) {
                        resp::error(res, 400, R"({"ok":false,"error":"task id is required"})");
                        return;
                    }
                    taskList.push_back({{"logical", jt["id"]}, {"task_id", jt["id"]}});
                    jt["run_id"] = runId;
                }
            } else if (r.rendered.contains("task") && r.rendered["task"].is_object()) {
                auto& t = r.rendered["task"];
                if (!t.contains("id") || !t["id"].is_string()) {
                    resp::error(res, 400, R"({"ok":false,"error":"task id is required"})");
                    return;
                }
                taskList.push_back({{"logical", t["id"]}, {"task_id", t["id"]}});
                t["run_id"] = runId;
            } else {
                if (!r.rendered.contains("id") || !r.rendered["id"].is_string()) {
                    resp::error(res, 400, R"({"ok":false,"error":"task id is required"})");
                    return;
                }
                taskList.push_back({{"logical", r.rendered["id"]}, {"task_id", r.rendered["id"]}});
                r.rendered["run_id"] = runId;
            }

            // 通过 DAG 线程池执行，避免为每个请求创建新的 detached 线程
            dag::DagThreadPool::instance().post([rendered = r.rendered, runId]() mutable {
                try {
                    dag::DagService::instance().runDag(rendered, runId);
                } catch (const std::exception& e) {
                    Logger::error(std::string("template run_async thread exception: ") + e.what());
                }
            });

            json data;
            data["run_id"] = runId;
            data["task_ids"] = taskList;
            resp::ok(res, data);
        } catch (const std::exception& e) {
            Logger::error(std::string("Failed to run template async: ") + e.what());
            resp::error(res, 500, std::string(R"({"ok":false,"error":")") + e.what() + R"("})");
            return;
        }
    }
} 
