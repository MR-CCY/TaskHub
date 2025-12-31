#include "cron_handler.h"
#include "scheduler/cron_scheduler.h"
#include "scheduler/cron_job.h"
#include "runner/task_config.h"
#include "log/logger.h"

// 按你项目里统一用的 json 头文件来
#include "json.hpp"

using json = nlohmann::json;

namespace taskhub {

using namespace taskhub::scheduler;
using namespace taskhub::core;

void CronHandler::setup_routes(httplib::Server& server)
{
    // 列表：GET /api/cron/jobs
    server.Get("/api/cron/jobs", &CronHandler::list_jobs);

    // 新增：POST /api/cron/jobs
    server.Post("/api/cron/jobs", &CronHandler::create_job);

    // 删除：DELETE /api/cron/jobs/:id
    server.Delete(R"(/api/cron/jobs/(\w+))", &CronHandler::delete_job);
}

// Request: GET /api/cron/jobs
// Response: 200 {"ok":true,"jobs":[{id,name,spec,enabled,target_type,next_time_epoch,summary:{exec_type,exec_command?}}]}
void CronHandler::list_jobs(const httplib::Request& req,
                            httplib::Response& res)
{
    auto& cron = CronScheduler::instance();
    auto jobs  = cron.listJobs();

    json j;
    j["ok"]   = true;
    j["jobs"] = json::array();

    for (const auto& job : jobs) {
        json item;
        item["id"]          = job.id;
        item["name"]        = job.name;
        item["spec"]        = job.spec;
        item["enabled"]     = job.enabled;
        item["target_type"] = (job.targetType == CronTargetType::SingleTask)
                                ? "SingleTask"
                                : (job.targetType == CronTargetType::Dag ? "Dag" : "Unknown");

        // next_time 简单转成时间戳（你后面可以封装成 ISO 字符串）
        auto ts = std::chrono::duration_cast<std::chrono::seconds>(
                      job.nextTime.time_since_epoch()).count();
        item["next_time_epoch"] = ts;

        // 简单 summary：如果是 SingleTask，就把 exec_type / exec_command 暴露一下
        json summary = json::object();
        if (job.targetType == CronTargetType::SingleTask &&
            job.taskTemplate.has_value()) {
            const auto& cfg = *job.taskTemplate;
            summary["exec_type"]    = TaskExecTypetoString(cfg.execType);
            summary["exec_command"] = cfg.execCommand;
        }
        item["summary"] = summary;

        j["jobs"].push_back(std::move(item));
    }

    res.set_content(j.dump(), "application/json; charset=utf-8");
}

// Request: POST /api/cron/jobs
//   Body JSON:
//     SingleTask: {"name":"job1","spec":"*/1 * * * *","target_type":"SingleTask","task":{"id":"task","name":"...","exec_type":"Local|Remote|Shell", "exec_command":"...", "timeout_ms":0,"retry_count":0,"priority":"Normal|Low|High|Critical"}}
//     Dag: {"name":"job1","spec":"*/1 * * * *","target_type":"Dag","dag":{"config":{"fail_policy":"SkipDownstream"|"FailFast","max_parallel":4},"tasks":[{"id":"a","deps":["b"],"name":"a","timeout_ms":0,"retry_count":0,"retryDelay":1000,"exec_type":"Local","exec_command":"..."}]}}
// Response:
//   200 {"ok":true,"job_id":"job_x"}
//   400 {"ok":false,"message":"..."}（缺字段/解析失败）
void CronHandler::create_job(const httplib::Request& req,
                             httplib::Response& res)
{
    try {
        json body = json::parse(req.body);

        // 1. 基本字段
        std::string name = body.value("name", "");
        std::string spec = body.value("spec", "");
        std::string targetTypeStr = body.value("target_type", "SingleTask");

        if (name.empty() || spec.empty()) {
            json err;
            err["ok"]      = false;
            err["message"] = "name/spec is required";
            res.status = 400;
            res.set_content(err.dump(), "application/json; charset=utf-8");
            return;
        }

        CronTargetType targetType = CronTargetType::SingleTask;
        if (targetTypeStr == "Dag") {
            targetType = CronTargetType::Dag;
        }

        // 2. 构造 CronJob（id 简单用 name + 时间戳，你也可以自己改成别的规则）
        auto now    = std::chrono::system_clock::now();
        auto ts_ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                          now.time_since_epoch()).count();
        std::string jobId = "job_" + std::to_string(ts_ms);

        CronJob job(jobId, name, spec, targetType);

        // 3. 根据 targetType 解析 payload
        if (targetType == CronTargetType::SingleTask) {
            if (!body.contains("task")) {
                json err;
                err["ok"]      = false;
                err["message"] = "SingleTask requires 'task' field";
                res.status = 400;
                res.set_content(err.dump(), "application/json; charset=utf-8");
                return;
            }

            const auto& t = body["task"];

            TaskConfig cfg;
            cfg.id.value  = t.value("id", "task");
            cfg.name      = t.value("name", name);

            std::string execTypeStr   = t.value("exec_type", "Local");
            cfg.execType              = StringToTaskExecType(execTypeStr);
            cfg.execCommand           = t.value("exec_command", "");

            // timeout_ms -> timeout
            int timeoutMs = t.value("timeout_ms", 0);
            if (timeoutMs > 0) {
                cfg.timeout = std::chrono::milliseconds(timeoutMs);
            }

            cfg.retryCount = t.value("retry_count", 0);

            // priority (accept string or int)
            TaskPriority priVal = TaskPriority::Normal;
            try {
                if (t.contains("priority")) {
                    if (t["priority"].is_string()) {
                        std::string pri = t["priority"].get<std::string>();
                        std::transform(pri.begin(), pri.end(), pri.begin(), [](unsigned char c){ return std::tolower(c); });
                        if (pri == "low") priVal = TaskPriority::Low;
                        else if (pri == "high") priVal = TaskPriority::High;
                        else if (pri == "critical") priVal = TaskPriority::Critical;
                    } else if (t["priority"].is_number_integer()) {
                        int p = t["priority"].get<int>();
                        if (p < static_cast<int>(TaskPriority::Low)) p = static_cast<int>(TaskPriority::Low);
                        if (p > static_cast<int>(TaskPriority::Critical)) p = static_cast<int>(TaskPriority::Critical);
                        priVal = static_cast<TaskPriority>(p);
                    }
                }
            } catch (...) {
                priVal = TaskPriority::Normal;
            }
            cfg.priority = priVal;

            job.targetType   = CronTargetType::SingleTask;
            job.taskTemplate = cfg;
        } else if (targetType == CronTargetType::Dag) {
            if (!body.contains("dag") || !body["dag"].is_object()) {
                json err;
                err["ok"]      = false;
                err["message"] = "Dag requires 'dag' object";
                res.status = 400;
                res.set_content(err.dump(), "application/json; charset=utf-8");
                return;
            }

            auto jdag = body["dag"];

            dag::DagConfig dagCfg;
            if (jdag.contains("config")) {
                auto jcfg = jdag["config"];
                std::string policy = jcfg.value("fail_policy", "SkipDownstream");
                dagCfg.failPolicy  = (policy == "FailFast")
                                        ? dag::FailPolicy::FailFast
                                        : dag::FailPolicy::SkipDownstream;
                dagCfg.maxParallel = jcfg.value("max_parallel", dagCfg.maxParallel);
            }

            if (!jdag.contains("tasks") || !jdag["tasks"].is_array()) {
                json err;
                err["ok"]      = false;
                err["message"] = "Dag requires 'tasks' array";
                res.status = 400;
                res.set_content(err.dump(), "application/json; charset=utf-8");
                return;
            }

            std::vector<dag::DagTaskSpec> specs;
            for (auto& jtask : jdag["tasks"]) {
                std::string idStr = jtask.value("id", "");
                if (idStr.empty()) {
                    continue;
                }

                dag::DagTaskSpec spec;
                core::TaskConfig cfgTask;
                cfgTask.id.value = idStr;
                spec.id          = cfgTask.id;

                if (jtask.contains("deps")) {
                    for (auto& d : jtask["deps"]) {
                        spec.deps.push_back(core::TaskId{ d.get<std::string>() });
                    }
                }

                cfgTask.name       = jtask.value("name", idStr);
                cfgTask.timeout    = std::chrono::milliseconds(jtask.value("timeout_ms", 0));
                cfgTask.retryCount = jtask.value("retry_count", 0);
                cfgTask.retryDelay = std::chrono::milliseconds(jtask.value("retryDelay", 1000));
                cfgTask.execType   = core::StringToTaskExecType(
                    jtask.value("exec_type", "Local"));
                cfgTask.execCommand = jtask.value("exec_command", "");

                spec.runnerCfg = cfgTask;
                specs.push_back(std::move(spec));
            }

            if (specs.empty()) {
                json err;
                err["ok"]      = false;
                err["message"] = "Dag tasks is empty";
                res.status = 400;
                res.set_content(err.dump(), "application/json; charset=utf-8");
                return;
            }

            dag::DagEventCallbacks callbacks;
            callbacks.onNodeStatusChanged = [](const core::TaskId&, core::TaskStatus) {};
            callbacks.onDagFinished       = [](bool) {};

            CronJob::DagJobPayload payload;
            payload.specs     = std::move(specs);
            payload.config    = dagCfg;
            payload.callbacks = callbacks;

            job.targetType = CronTargetType::Dag;
            job.dagPayload = std::move(payload);
        }

        // 4. 加入 CronScheduler
        auto& cron = CronScheduler::instance();
        cron.addJob(job);

        json ok;
        ok["ok"]     = true;
        ok["job_id"] = job.id;
        res.set_content(ok.dump(), "application/json; charset=utf-8");
    }
    catch (const std::exception& ex) {
        Logger::error(std::string("CronHandler::create_job exception: ") + ex.what());
        json err;
        err["ok"]      = false;
        err["message"] = std::string("invalid json: ") + ex.what();
        res.status = 400;
        res.set_content(err.dump(), "application/json; charset=utf-8");
    }
}

// Request: DELETE /api/cron/jobs/{id}
// Response: 200 {"ok":true}; 400 {"ok":false,"message":"missing job id"}
void CronHandler::delete_job(const httplib::Request& req,
                             httplib::Response& res)
{
    // 路由里我们用了 R"(/api/cron/jobs/(\w+))"，id 在 matches[1]
    if (req.matches.size() < 2) {
        json err;
        err["ok"]      = false;
        err["message"] = "missing job id";
        res.status = 400;
        res.set_content(err.dump(), "application/json; charset=utf-8");
        return;
    }

    std::string jobId = req.matches[1];

    auto& cron = CronScheduler::instance();
    cron.removeJob(jobId);

    json ok;
    ok["ok"] = true;
    res.set_content(ok.dump(), "application/json; charset=utf-8");
}

} // namespace taskhub::handlers
