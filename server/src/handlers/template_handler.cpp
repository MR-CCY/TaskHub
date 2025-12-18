#include "template_handler.h"
#include "auth/auth_manager.h"
#include "httplib.h"
#include "log/logger.h"
#include "json.hpp"
#include "template/template_store.h"
#include "template/template_renderer.h"
#include "core/http_response.h"
#include "dag/dag_service.h"

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
    }

    // Request: POST /template
    //   Body: {"template_id":"t1","name":"echo template","description":"...","task_json_template":{...},"schema":{...}}
    // Response: {"ok":true,"template_id":"t1"} or {"ok":false,"error":"..."} (duplicate id / missing fields / bad json).
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
    // Response: {"ok":true,"data":{template json}} or {"ok":false,"error":"template not found"}.
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
    // Response: {"ok":true,"data":[{template...}]}
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
    // Response: {"ok":true} or {"ok":false,"error":"template not found"}.
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
    //   Body: partial fields to update (name/description/task_json_template/schema)
    // Response: {"ok":true} or {"ok":false,"error":"..."} (not yet implemented).
    void TemplateHandler::update(const httplib::Request &req, httplib::Response &res)
    {
        // TODO(M13.3): implement update
    }
    // Request: POST /template/render
    //   Body: {"template_id":"t1","params":{...}}
    // Response: {"ok":true,"data":{rendered task json}}; {"ok":false,"error":"..."} on validation/render failure.
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
    //   Body: {"template_id":"t1","params":{...}}
    // Response: {"ok":true,"nodes":{...},"summary":{total,success,failed,skipped}} or {"ok":false,"error":"..."}.
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
            auto dagResult= dag::DagService::instance().runDag(r.rendered);
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
}
