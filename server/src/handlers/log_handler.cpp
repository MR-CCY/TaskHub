#include "log_handler.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <json.hpp>
// NOTE: adjust include path if your LogManager header lives elsewhere
#include "core/log_manager.h"

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

void LogHandler::logs(const httplib::Request &req, httplib::Response &res)
{
    // ---- parse query params ----
    // required
    if (!req.has_param("task_id")) {
        res.status = 400;
        res.set_content(R"({"ok":false,"code":400,"message":"missing task_id"})", "application/json");
        return;
    }

    const std::string taskId = req.get_param_value("task_id");
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
            taskhub::core::TaskId{taskId}, from, static_cast<std::size_t>(limit));
        records = std::move(qr.records);
        nextFrom = qr.nextFrom;
    } catch (const std::exception& e) {
        json err;
        err["ok"] = false;
        err["code"] = 500;
        err["message"] = std::string("log query failed: ") + e.what();
        res.status = 500;
        res.set_content(err.dump(), "application/json");
        return;
    } catch (...) {
        res.status = 500;
        res.set_content(R"({"ok":false,"code":500,"message":"log query failed: unknown"})", "application/json");
        return;
    }

    // ---- build response ----
    json j;
    j["ok"] = true;
    j["task_id"] = taskId;
    j["from"] = from;
    j["limit"] = limit;
    j["next_from"] = nextFrom;

    json arr = json::array();
    for (const auto& r : records) {
        arr.push_back(logRecordToJson(r));
    }
    j["records"] = std::move(arr);

    res.status = 200;
    // dump 时替换非法 UTF-8，避免异常终止
    std::string body = j.dump(-1, ' ', false, json::error_handler_t::replace);
    res.set_content(body, "application/json");
}


} // namespace taskhub
