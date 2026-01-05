#include "log_handler.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <json.hpp>
// NOTE: adjust include path if your LogManager header lives elsewhere
#include "log/log_manager.h"
#include "core/http_response.h"

namespace taskhub {

using json = nlohmann::json;

static std::uint64_t parse_u64_or(const std::string& s, std::uint64_t def) {
    try {
        if (s.empty()) return def;
        return static_cast<std::uint64_t>(std::stoull(s));
    } catch (...) {
        return def;
    }
}

static int parse_i32_or(const std::string& s, int def) {
    try {
        if (s.empty()) return def;
        return std::stoi(s);
    } catch (...) {
        return def;
    }
}

// Minimal JSON conversion for LogRecord.
// If you already have a shared serializer, feel free to replace this.
static json logRecordToJson(const taskhub::core::LogRecord& r) {
    json j;
    j["seq"] = r.seq;

    // identity
    j["task_id"] = r.taskId.value;
    if (!r.taskId.runId.empty()) j["run_id"] = r.taskId.runId;
    if (!r.dagRunId.empty())  j["dag_run_id"] = r.dagRunId;
    if (!r.cronJobId.empty()) j["cron_job_id"] = r.cronJobId;
    if (!r.workerId.empty())  j["worker_id"] = r.workerId;

    // content
    j["level"]  = static_cast<int>(r.level);
    j["stream"] = static_cast<int>(r.stream);
    j["message"] = r.message;

    // timing
    j["ts_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                      r.ts.time_since_epoch())
                      .count();
    j["duration_ms"] = r.durationMs;
    j["attempt"] = r.attempt;

    // extra fields
    if (!r.fields.empty()) {
        json jf = json::object();
        for (const auto& kv : r.fields) {
            jf[kv.first] = kv.second;
        }
        j["fields"] = std::move(jf);
    } else {
        j["fields"] = json::object();
    }

    return j;
}

void LogHandler::setup_routes(httplib::Server &server)
{
    // GET /api/tasks/logs?task_id=xxx&from=1&limit=200
    server.Get("/api/tasks/logs", &LogHandler::logs);
}

// Request: GET /api/tasks/logs?task_id=<id>&run_id=<rid?>&from=<seq?>&limit=<n?>
//   from: 默认 1；limit: 默认 200，范围 1..2000
// Response:
//   200 {"code":0,"message":"ok","data":{"task_id":string,"from":u64,"limit":int,"next_from":u64,"records":[{seq,task_id,run_id?,dag_run_id?,cron_job_id?,worker_id?,level:int,stream:int,message,ts_ms,duration_ms,attempt,fields:{}}]}}
//   400 {"code":400,"message":"missing task_id","data":null}; 500 {"code":500,"message":"log query failed: ...","data":null}
void LogHandler::logs(const httplib::Request &req, httplib::Response &res)
{
    // ---- parse query params ----
    // required
    if (!req.has_param("task_id")) {
        resp::bad_request(res, "missing task_id");
        return;
    }

    const std::string taskId = req.get_param_value("task_id");
    const std::string runId  = req.has_param("run_id") ? req.get_param_value("run_id") : "";
    const std::uint64_t from = req.has_param("from")
        ? parse_u64_or(req.get_param_value("from"), 1)
        : 1;

    int limit = req.has_param("limit")
        ? parse_i32_or(req.get_param_value("limit"), 200)
        : 200;
    // sane bounds
    limit = std::clamp(limit, 1, 2000);

    // ---- query LogManager ----
    // Expected semantics:
    // - from: the starting seq (inclusive), usually 1
    // - limit: max records
    // - returns: records + next_from (the next seq to request)
    std::vector<taskhub::core::LogRecord> records;
    std::uint64_t nextFrom = from;

    try {
        auto qr = taskhub::core::LogManager::instance().query(
            taskhub::core::TaskId{taskId, runId}, from, static_cast<std::size_t>(limit));
        records = std::move(qr.records);
        nextFrom = qr.nextFrom;
    } catch (const std::exception& e) {
        resp::error(res, 500, std::string("log query failed: ") + e.what(), 500);
        return;
    } catch (...) {
        resp::error(res, 500, "log query failed: unknown", 500);
        return;
    }

    // ---- build response ----
    json data;
    data["task_id"] = taskId;
    data["from"] = from;
    data["limit"] = limit;
    data["next_from"] = nextFrom;

    json arr = json::array();
    for (const auto& r : records) {
        arr.push_back(logRecordToJson(r));
    }
    data["records"] = std::move(arr);

    // 统一响应
    resp::ok(res, data);
}


} // namespace taskhub
