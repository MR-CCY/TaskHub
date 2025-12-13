#include "worker_handler.h"
#include "worker/worker_registry.h"
#include "core/logger.h"
#include "json.hpp"
#include "runner/task_config.h"
#include "runner/taskRunner.h"
#include "runner/task_result.h"
#include "core/http_response.h"
#include <chrono>

using json = nlohmann::json;

namespace taskhub
{ 
    using taskhub::worker::WorkerRegistry;
    using taskhub::worker::WorkerInfo;
    
    void WorkHandler::setup_routes(httplib::Server& server)
    {
        // TODO(M11.2): Worker 注册接口
        // POST /api/workers/register
        server.Post("/api/workers/register", &WorkHandler::workers_register);
    
        // TODO(M11.2): Worker 心跳接口
        // POST /api/workers/heartbeat
        server.Post("/api/workers/heartbeat", &WorkHandler::workers_heartbeat);
    
        // TODO(M11.3+):（可选）Worker 列表接口
        // GET /api/workers
        server.Get("/api/workers", &WorkHandler::workers_list);

        server.Post("/api/worker/execute", &WorkHandler::worker_execute);
    }
    void WorkHandler::workers_register(const httplib::Request &req, httplib::Response &res)
    {
        Logger::info("Worker register request: " + req.body);
        try {
            json jReq = json::parse(req.body, nullptr, false);
            if (jReq.is_discarded() || !jReq.is_object()) {
                resp::error(res, 400, "invalid json", 400);
                return;
            }

            WorkerInfo info;
            info.id   = jReq.value("id", std::string{});
            info.host = jReq.value("host", std::string{"127.0.0.1"});
            info.port = jReq.value("port", 0);

            // queues
            if (jReq.contains("queues") && jReq["queues"].is_array()) {
                for (const auto& q : jReq["queues"]) {
                    if (q.is_string()) info.queues.push_back(q.get<std::string>());
                }
            }

            // labels (optional)
            if (jReq.contains("labels") && jReq["labels"].is_array()) {
                for (const auto& lb : jReq["labels"]) {
                    if (lb.is_string()) info.labels.push_back(lb.get<std::string>());
                }
            }

            info.runningTasks  = jReq.value("running_tasks", 0);
            info.lastHeartbeat = std::chrono::steady_clock::now();

            if (info.id.empty() || info.port <= 0) {
                resp::error(res, 400, "missing required fields: id/port", 400);
                return;
            }

            WorkerRegistry::instance().upsertWorker(info);

            Logger::info("Worker registered: id=" + info.id + ", host=" + info.host + ", port=" + std::to_string(info.port));

            json j;
            j["ok"] = true;
            res.status = 200;
            res.set_content(j.dump(), "application/json");
        } catch (const std::exception& e) {
            resp::error(res, 500, std::string("internal error: ") + e.what(), 500);
        } catch (...) {
            resp::error(res, 500, "internal error: unknown", 500);
        }
    }
    void WorkHandler::workers_heartbeat(const httplib::Request &req, httplib::Response &res)
    {
       Logger::info("Worker heartbeat request: " + req.body);
        try {
            auto j = json::parse(req.body);
    
            std::string id = j.value("id", "");
            int running = j.value("running_tasks", 0);
    
            if (id.empty()) {
                resp::error(res, 400, "missing worker id", 400);
                return;
            }
    
            bool ok = WorkerRegistry::instance().touchHeartbeat(id, running);
            if (!ok) {
                resp::error(res, 404, "worker not found", 404);
                return;
            }
    
            res.set_content(R"({"ok":true})", "application/json");
        } catch (...) {
            resp::error(res, 500, "internal error", 500);
        }
    }
    void WorkHandler::workers_list(const httplib::Request &req, httplib::Response &res)
    {
        Logger::info("Worker list request");
        try {
            auto workers = WorkerRegistry::instance().listWorkers();

            json arr = json::array();
            for (const auto& w : workers) {
                json jw;
                jw["id"] = w.id;
                jw["host"] = w.host;
                jw["port"] = w.port;
                jw["running_tasks"] = w.runningTasks;
                jw["alive"] = w.isAlive();

                auto now = std::chrono::steady_clock::now();
                auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - w.lastHeartbeat).count();
                jw["last_seen_ms_ago"] = diff;

                jw["queues"] = json::array();
                for (const auto& q : w.queues) jw["queues"].push_back(q);

                jw["labels"] = json::array();
                for (const auto& lb : w.labels) jw["labels"].push_back(lb);

                arr.push_back(jw);
            }

            json j;
            j["ok"] = true;
            j["workers"] = arr;

            res.status = 200;
            res.set_content(j.dump(), "application/json");
        } catch (const std::exception& e) {
            resp::error(res, 500, std::string("internal error: ") + e.what(), 500);
        } catch (...) {
            resp::error(res, 500, "internal error: unknown", 500);
        }
    }
    void WorkHandler::worker_execute(const httplib::Request &req, httplib::Response &res)
    {
        Logger::info("Worker execute task request: " + req.body);
        try {
            json jReq = json::parse(req.body, nullptr, false);
            if (jReq.is_discarded()) {
                res.status = 400;
                res.set_content(R"({"code":400,"message":"invalid json"})", "application/json");
                resp::error(res, 400, jReq.dump(), 400);
                return;
            }
            // 2) json -> TaskConfig
            core::TaskConfig cfg;
            
            cfg = core::parseTaskConfigFromReq(jReq);
            if (cfg.execType == core::TaskExecType::Remote) {
                resp::error(res, 400, "worker cannot execute Remote task", 400);
                return;
            }
            Logger::info("Worker execute task: id=" + cfg.id.value +
                        ", name=" + cfg.name +
                        ", execType=" + core::TaskExecTypetoString(cfg.execType));
            core::TaskResult r;
            
            r = runner::TaskRunner::instance().run(cfg, nullptr);
           

            // 4) return JSON
            json jResp = taskResultToJson(r);
            res.status = 200;
            Logger::info("Worker execute task finished: id=" + cfg.id.value +
                        ", status=" + std::to_string(static_cast<int>(r.status)) +
                        ", durationMs=" +  std::to_string(r.durationMs));
            res.set_content(jResp.dump(), "application/json");
        } catch (const std::exception& e) {
            // 兜底：任何未预期异常返回 500，避免线程挂掉
            resp::error(res, 500, std::string("internal error: ") + e.what(), 500);
        } catch (...) {
            resp::error(res, 500, "internal error: unknown", 500);
        }
    }
}
