#include "task_event_repo.h"
#include "db.h"
#include "log/logger.h"
#include <sqlite3.h>
#include <mutex>

using json = nlohmann::json;
namespace {
std::mutex g_task_event_mutex;
}

namespace taskhub {

TaskEventRepo& TaskEventRepo::instance()
{
    static TaskEventRepo repo;
    return repo;
}

void TaskEventRepo::insertFromJson(const json& eventJson)
{
    std::lock_guard<std::mutex> lk(g_task_event_mutex);
    sqlite3* db = Db::instance().handle();
    if (!db) {
        Logger::error("insertFromJson failed: DB handle is null");
        return;
    }

    const char* sql =
        "INSERT INTO task_event (run_id, task_id, type, event, ts_ms, payload_json) "
        "VALUES (?,?,?,?,?,?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Logger::error("task_event insert prepare failed: " + Db::instance().last_error());
        return;
    }

    const std::string runId  = eventJson.value("run_id", std::string{});
    const std::string taskId = eventJson.value("task_id", std::string{});
    const std::string type   = eventJson.value("type", std::string{});
    const std::string event  = eventJson.value("event", std::string{});
    const long long ts       = eventJson.value("ts_ms", 0LL);
    const std::string payload = eventJson.dump();

    sqlite3_bind_text(stmt, 1, runId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, taskId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, event.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(ts));
    sqlite3_bind_text(stmt, 6, payload.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        Logger::error("task_event insert step failed: " + Db::instance().last_error());
    }

    sqlite3_finalize(stmt);
}

std::vector<TaskEventRepo::EventRow> TaskEventRepo::query(const std::string& runId,
                                                         const std::string& taskId,
                                                         const std::string& type,
                                                         const std::string& event,
                                                         long long startTs,
                                                         long long endTs,
                                                         int limit)
{
    std::vector<EventRow> rows;
    sqlite3* db = Db::instance().handle();
    if (!db) {
        Logger::error("task_event query failed: DB handle is null");
        return rows;
    }

    std::string sql = "SELECT id, run_id, task_id, type, event, ts_ms, payload_json FROM task_event WHERE 1=1";
    std::vector<std::string> params;
    std::vector<long long> intParams;

    if (!runId.empty()) {
        sql += " AND run_id = ?";
        params.push_back(runId);
    }
    if (!taskId.empty()) {
        sql += " AND task_id = ?";
        params.push_back(taskId);
    }
    if (!type.empty()) {
        sql += " AND type = ?";
        params.push_back(type);
    }
    if (!event.empty()) {
        sql += " AND event = ?";
        params.push_back(event);
    }
    if (startTs > 0) {
        sql += " AND ts_ms >= ?";
        intParams.push_back(startTs);
    }
    if (endTs > 0) {
        sql += " AND ts_ms <= ?";
        intParams.push_back(endTs);
    }
    sql += " ORDER BY ts_ms DESC";
    if (limit > 0) {
        sql += " LIMIT " + std::to_string(limit);
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        Logger::error("task_event query prepare failed: " + Db::instance().last_error());
        return rows;
    }

    int idx = 1;
    for (const auto& p : params) {
        sqlite3_bind_text(stmt, idx++, p.c_str(), -1, SQLITE_TRANSIENT);
    }
    for (const auto v : intParams) {
        sqlite3_bind_int64(stmt, idx++, static_cast<sqlite3_int64>(v));
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        EventRow r;
        r.id      = sqlite3_column_int64(stmt, 0);
        r.runId   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        r.taskId  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        r.type    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        r.event   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        r.tsMs    = sqlite3_column_int64(stmt, 5);
        const char* payload = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        if (payload) {
            try {
                r.payload = json::parse(payload);
            } catch (...) {
                r.payload = json::object();
                r.payload["raw"] = payload;
            }
        } else {
            r.payload = json::object();
        }
        rows.push_back(std::move(r));
    }

    sqlite3_finalize(stmt);
    return rows;
}

} // namespace taskhub
