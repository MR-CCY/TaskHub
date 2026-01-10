#include "task_run_repo.h"
#include "db.h"
#include "log/logger.h"
#include <sqlite3.h>
#include <mutex>
#include <vector>

using json = nlohmann::json;
namespace {
std::mutex g_task_run_mutex;
}

namespace taskhub {

TaskRunRepo& TaskRunRepo::instance()
{
    static TaskRunRepo repo;
    return repo;
}

int TaskRunRepo::statusToInt(core::TaskStatus st) const
{
    switch (st) {
    case core::TaskStatus::Pending: return 0;
    case core::TaskStatus::Running: return 1;
    case core::TaskStatus::Success: return 2;
    case core::TaskStatus::Failed:  return 3;
    case core::TaskStatus::Skipped: return 4;
    case core::TaskStatus::Canceled:return 5;
    case core::TaskStatus::Timeout: return 6;
    default: return 0;
    }
}

void TaskRunRepo::upsertFromTaskJson(const std::string& runId, const json& jtask)
{
    std::lock_guard<std::mutex> lk(g_task_run_mutex);

    sqlite3* db = Db::instance().handle();
    if (!db) {
        Logger::error("upsertFromTaskJson failed: DB handle is null");
        return;
    }

    const char* sql =
        "INSERT INTO task_run ("
        "run_id, logical_id, task_id, name, exec_type, exec_command, "
        "exec_params_json, deps_json, status, attempt, max_attempts"
        ") VALUES (?,?,?,?,?,?,?,?,?,?,?) "
        "ON CONFLICT(run_id, logical_id) DO UPDATE SET "
        "task_id=excluded.task_id, "
        "name=excluded.name, "
        "exec_type=excluded.exec_type, "
        "exec_command=excluded.exec_command, "
        "exec_params_json=excluded.exec_params_json, "
        "deps_json=excluded.deps_json, "
        "status=excluded.status, "
        "attempt=excluded.attempt, "
        "max_attempts=excluded.max_attempts;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Logger::error("task_run upsert prepare failed: " + Db::instance().last_error());
        return;
    }

    auto getStr = [](const json& j, const std::string& key, const std::string& def) -> std::string {
        if (!j.contains(key)) return def;
        const auto& v = j.at(key);
        if (v.is_string()) return v.get<std::string>();
        return v.dump();
    };
    auto getInt = [](const json& j, const std::string& key, int def) -> int {
        if (!j.contains(key)) return def;
        const auto& v = j.at(key);
        if (v.is_number_integer()) return v.get<int>();
        if (v.is_number()) return static_cast<int>(v.get<double>());
        if (v.is_string()) {
            try { return std::stoi(v.get<std::string>()); }
            catch (...) { return def; }
        }
        return def;
    };

    const std::string logicalId = getStr(jtask, "id", "");
    const std::string taskId    = getStr(jtask, "task_id", logicalId);
    const std::string name      = getStr(jtask, "name", "");
    const std::string execType  = getStr(jtask, "exec_type", "");
    const std::string execCmd   = getStr(jtask, "exec_command", "");
    const std::string execParams = (jtask.contains("exec_params") && jtask["exec_params"].is_object())
        ? jtask["exec_params"].dump()
        : std::string("{}");
    const std::string depsJson = (jtask.contains("deps") && jtask["deps"].is_array())
        ? jtask["deps"].dump()
        : std::string("[]");
    const int maxAttempts = getInt(jtask, "max_attempts", getInt(jtask, "retry_count", 1));

    if (logicalId.empty()) {
        Logger::error("task_run upsert failed: logical id is empty");
        sqlite3_finalize(stmt);
        return;
    }

    sqlite3_bind_text(stmt, 1, runId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, logicalId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, taskId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, execType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, execCmd.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, execParams.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, depsJson.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 9, statusToInt(core::TaskStatus::Pending));
    sqlite3_bind_int(stmt, 10, 1);
    sqlite3_bind_int(stmt, 11, maxAttempts <= 0 ? 1 : maxAttempts);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        Logger::error("task_run upsert step failed: " + Db::instance().last_error());
    }

    sqlite3_finalize(stmt);
}

void TaskRunRepo::markRunning(const std::string& runId, const std::string& logicalId, long long tsMs)
{
    std::lock_guard<std::mutex> lk(g_task_run_mutex);

    sqlite3* db = Db::instance().handle();
    if (!db) {
        Logger::error("markRunning failed: DB handle is null");
        return;
    }

    const char* sql = "UPDATE task_run SET status=?, start_ts_ms=? WHERE run_id=? AND logical_id=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Logger::error("task_run markRunning prepare failed: " + Db::instance().last_error());
        return;
    }

    sqlite3_bind_int(stmt, 1, statusToInt(core::TaskStatus::Running));
    sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(tsMs));
    sqlite3_bind_text(stmt, 3, runId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, logicalId.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        Logger::error("task_run markRunning step failed: " + Db::instance().last_error());
    }

    sqlite3_finalize(stmt);
}

void TaskRunRepo::markFinished(const std::string& runId, const std::string& logicalId, const core::TaskResult& r, long long tsMs)
{
    std::lock_guard<std::mutex> lk(g_task_run_mutex);

    sqlite3* db = Db::instance().handle();
    if (!db) {
        Logger::error("markFinished failed: DB handle is null");
        return;
    }

    const char* sql =
        "UPDATE task_run SET "
        "status=?, exit_code=?, duration_ms=?, message=?, stdout=?, stderr=?, "
        "attempt=?, max_attempts=?, worker_id=?, worker_host=?, worker_port=?, end_ts_ms=?, metadata_json=? "
        "WHERE run_id=? AND logical_id=?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Logger::error("task_run markFinished prepare failed: " + Db::instance().last_error());
        return;
    }

    json meta = json::object();
    for (const auto& kv : r.metadata) {
        meta[kv.first] = kv.second;
    }

    sqlite3_bind_int(stmt, 1, statusToInt(r.status));
    sqlite3_bind_int(stmt, 2, r.exitCode);
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(r.durationMs));
    sqlite3_bind_text(stmt, 4, r.message.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, r.stdoutData.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, r.stderrData.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, r.attempt);
    sqlite3_bind_int(stmt, 8, r.maxAttempts);
    sqlite3_bind_text(stmt, 9, r.workerId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 10, r.workerHost.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 11, r.workerPort);
    sqlite3_bind_int64(stmt, 12, static_cast<sqlite3_int64>(tsMs));
    const std::string metaStr = meta.dump();
    sqlite3_bind_text(stmt, 13, metaStr.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 14, runId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 15, logicalId.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        Logger::error("task_run markFinished step failed: " + Db::instance().last_error());
    }

    sqlite3_finalize(stmt);
}

void TaskRunRepo::markSkipped(const std::string& runId, const std::string& logicalId, const std::string& reason, long long tsMs)
{
    std::lock_guard<std::mutex> lk(g_task_run_mutex);

    sqlite3* db = Db::instance().handle();
    if (!db) {
        Logger::error("markSkipped failed: DB handle is null");
        return;
    }

    const char* sql =
        "UPDATE task_run SET status=?, message=?, end_ts_ms=? "
        "WHERE run_id=? AND logical_id=?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Logger::error("task_run markSkipped prepare failed: " + Db::instance().last_error());
        return;
    }

    sqlite3_bind_int(stmt, 1, statusToInt(core::TaskStatus::Skipped));
    sqlite3_bind_text(stmt, 2, reason.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(tsMs));
    sqlite3_bind_text(stmt, 4, runId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, logicalId.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        Logger::error("task_run markSkipped step failed: " + Db::instance().last_error());
    }

    sqlite3_finalize(stmt);
}

std::vector<TaskRunRepo::TaskRunRow> TaskRunRepo::query(const std::string& runId,const std::string& name,int limit)
{
    std::vector<TaskRunRow> rows;
    sqlite3* db = Db::instance().handle();
    if (!db) {
        Logger::error("task_run query failed: DB handle is null");
        return rows;
    }

    std::string sql =
        "SELECT id, run_id, logical_id, task_id, name, exec_type, exec_command, exec_params_json, deps_json, "
        "status, exit_code, duration_ms, message, stdout, stderr, attempt, max_attempts, start_ts_ms, end_ts_ms, "
        "worker_id, worker_host, worker_port "
        "FROM task_run WHERE 1=1";

    std::vector<std::string> params;
    if (!runId.empty()) {
        sql += " AND run_id = ?";
        params.push_back(runId);
    }
    if (!name.empty()) {
        sql += " AND name = ?";
        params.push_back(name);
    }
    sql += " ORDER BY id DESC";
    if (limit > 0) {
        sql += " LIMIT " + std::to_string(limit);
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        Logger::error("task_run query prepare failed: " + Db::instance().last_error());
        return rows;
    }

    int idx = 1;
    for (const auto& p : params) {
        sqlite3_bind_text(stmt, idx++, p.c_str(), -1, SQLITE_TRANSIENT);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        TaskRunRow r;
        r.id            = sqlite3_column_int(stmt, 0);
        r.runId         = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        r.logicalId     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        r.taskId        = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        r.name          = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        r.execType      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        r.execCommand   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        r.execParamsJson= reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        r.depsJson      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        r.status        = sqlite3_column_int(stmt, 9);
        r.exitCode      = sqlite3_column_int(stmt, 10);
        r.durationMs    = sqlite3_column_int64(stmt, 11);
        const unsigned char* msg = sqlite3_column_text(stmt, 12);
        r.message       = msg ? reinterpret_cast<const char*>(msg) : "";
        const unsigned char* out = sqlite3_column_text(stmt, 13);
        r.stdoutText    = out ? reinterpret_cast<const char*>(out) : "";
        const unsigned char* err = sqlite3_column_text(stmt, 14);
        r.stderrText    = err ? reinterpret_cast<const char*>(err) : "";
        r.attempt       = sqlite3_column_int(stmt, 15);
        r.maxAttempts   = sqlite3_column_int(stmt, 16);
        r.startTsMs     = sqlite3_column_int64(stmt, 17);
        r.endTsMs       = sqlite3_column_int64(stmt, 18);
        r.workerId      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 19));
        r.workerHost    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 20));
        r.workerPort    = sqlite3_column_int(stmt, 21);
        rows.push_back(std::move(r));
    }

    sqlite3_finalize(stmt);
    return rows;
}

std::optional<TaskRunRepo::TaskRunRow> TaskRunRepo::get(const std::string& runId, const std::string& logicalId)
{
    sqlite3* db = Db::instance().handle();
    if (!db) return std::nullopt;

    const char* sql =
        "SELECT id, run_id, logical_id, task_id, name, exec_type, exec_command, exec_params_json, deps_json, "
        "status, exit_code, duration_ms, message, stdout, stderr, attempt, max_attempts, start_ts_ms, end_ts_ms, "
        "worker_id, worker_host, worker_port,metadata_json "
        "FROM task_run WHERE run_id=? AND logical_id=?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, runId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, logicalId.c_str(), -1, SQLITE_TRANSIENT);

    std::optional<TaskRunRow> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        TaskRunRow r;
        r.id            = sqlite3_column_int(stmt, 0);
        r.runId         = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        r.logicalId     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        r.taskId        = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        r.name          = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        r.execType      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        r.execCommand   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        r.execParamsJson= reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        r.depsJson      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        r.status        = sqlite3_column_int(stmt, 9);
        r.exitCode      = sqlite3_column_int(stmt, 10);
        r.durationMs    = sqlite3_column_int64(stmt, 11);
        const unsigned char* msg = sqlite3_column_text(stmt, 12);
        r.message       = msg ? reinterpret_cast<const char*>(msg) : "";
        const unsigned char* out = sqlite3_column_text(stmt, 13);
        r.stdoutText    = out ? reinterpret_cast<const char*>(out) : "";
        const unsigned char* err = sqlite3_column_text(stmt, 14);
        r.stderrText    = err ? reinterpret_cast<const char*>(err) : "";
        r.attempt       = sqlite3_column_int(stmt, 15);
        r.maxAttempts   = sqlite3_column_int(stmt, 16);
        r.startTsMs     = sqlite3_column_int64(stmt, 17);
        r.endTsMs       = sqlite3_column_int64(stmt, 18);
        r.workerId      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 19));
        r.workerHost    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 20));
        r.workerPort    = sqlite3_column_int(stmt, 21);
        r.metadataJson    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 22));
        result = std::move(r);
    }
    sqlite3_finalize(stmt);
    return result;
}

} // namespace taskhub
