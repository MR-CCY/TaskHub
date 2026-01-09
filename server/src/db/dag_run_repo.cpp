#include "dag_run_repo.h"
#include "db.h"
#include "log/logger.h"
#include "utils/utils.h"
#include <sqlite3.h>
#include <mutex>
#include <vector>

using json = nlohmann::json;
namespace {
std::mutex g_dag_run_mutex;
}

namespace taskhub {

DagRunRepo& DagRunRepo::instance()
{
    static DagRunRepo repo;
    return repo;
}

void DagRunRepo::insertRun(const std::string& runId,const json& body,const std::string& source,const std::string& workflowJson)
{
    std::lock_guard<std::mutex> lk(g_dag_run_mutex);

    sqlite3* db = Db::instance().handle();
    if (!db) {
        Logger::error("insertRun failed: DB handle is null");
        return;
    }

    const char* sql =
        "INSERT OR REPLACE INTO dag_run ("
        "run_id, name, source, template_id, cron_job_id, fail_policy, max_parallel, "
        "status, message, start_ts_ms, end_ts_ms, dag_json, workflow_json, "
        "total, success_count, failed_count, skipped_count"
        ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Logger::error("dag_run insert prepare failed: " + Db::instance().last_error());
        return;
    }

    const auto startTs = utils::now_millis();
    const auto jcfg = (body.contains("config") && body["config"].is_object()) ? body["config"] : json::object();
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
    const std::string failPolicy = getStr(jcfg, "fail_policy", "SkipDownstream");
    const int maxParallel = getInt(jcfg, "max_parallel", 4);
    const std::string templateId = getStr(jcfg, "template_id", "");
    const std::string cronJobId  = getStr(jcfg, "cron_job_id", "");
    const std::string dagNameCfg = getStr(jcfg, "name", "");
    const std::string dagNameBody = getStr(body, "name", "");
    const std::string dagName    = !dagNameBody.empty() ? dagNameBody : dagNameCfg;
    const int total = body.contains("tasks") && body["tasks"].is_array() ? static_cast<int>(body["tasks"].size()) : 0;
    const std::string dagJson = body.dump();

    sqlite3_bind_text(stmt, 1, runId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, dagName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, source.c_str(), -1, SQLITE_TRANSIENT);

    if (templateId.empty()) sqlite3_bind_null(stmt, 4);
    else sqlite3_bind_text(stmt, 4, templateId.c_str(), -1, SQLITE_TRANSIENT);

    if (cronJobId.empty()) sqlite3_bind_null(stmt, 5);
    else sqlite3_bind_text(stmt, 5, cronJobId.c_str(), -1, SQLITE_TRANSIENT);

    sqlite3_bind_text(stmt, 6, failPolicy.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, maxParallel);
    sqlite3_bind_int(stmt, 8, 0); // Running
    sqlite3_bind_text(stmt, 9, "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 10, static_cast<sqlite3_int64>(startTs));
    sqlite3_bind_int64(stmt, 11, 0);
    sqlite3_bind_text(stmt, 12, dagJson.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 13, workflowJson.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 14, total);
    sqlite3_bind_int(stmt, 15, 0);
    sqlite3_bind_int(stmt, 16, 0);
    sqlite3_bind_int(stmt, 17, 0);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        Logger::error("dag_run insert step failed: " + Db::instance().last_error());
    }
    sqlite3_finalize(stmt);
}

void DagRunRepo::finishRun(const std::string& runId,
                           int status,
                           long long endTsMs,
                           int total,
                           int ok,
                           int failed,
                           int skipped,
                           const std::string& message)
{
    std::lock_guard<std::mutex> lk(g_dag_run_mutex);

    sqlite3* db = Db::instance().handle();
    if (!db) {
        Logger::error("finishRun failed: DB handle is null");
        return;
    }

    const char* sql =
        "UPDATE dag_run SET "
        "status=?, end_ts_ms=?, total=?, success_count=?, failed_count=?, skipped_count=?, message=? "
        "WHERE run_id=?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Logger::error("dag_run finish prepare failed: " + Db::instance().last_error());
        return;
    }

    sqlite3_bind_int(stmt, 1, status);
    sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(endTsMs));
    sqlite3_bind_int(stmt, 3, total);
    sqlite3_bind_int(stmt, 4, ok);
    sqlite3_bind_int(stmt, 5, failed);
    sqlite3_bind_int(stmt, 6, skipped);
    sqlite3_bind_text(stmt, 7, message.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, runId.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        Logger::error("dag_run finish step failed: " + Db::instance().last_error());
    }

    sqlite3_finalize(stmt);
}

std::vector<DagRunRepo::DagRunRow> DagRunRepo::query(const std::string& runId,
                                                    const std::string& nameLike,
                                                    long long startTs,
                                                    long long endTs,
                                                    int limit)
{
    std::vector<DagRunRow> rows;
    sqlite3* db = Db::instance().handle();
    if (!db) {
        Logger::error("dag_run query failed: DB handle is null");
        return rows;
    }

    std::string sql =
        "SELECT run_id, name, source, status, start_ts_ms, end_ts_ms, total, success_count, failed_count, skipped_count, message,dag_json, workflow_json "
        "FROM dag_run WHERE 1=1";

    std::vector<std::string> params;
    std::vector<long long> intParams;
    if (!runId.empty()) {
        sql += " AND run_id = ?";
        params.push_back(runId);
    }
    if (!nameLike.empty()) {
        sql += " AND name LIKE ?";
        params.push_back("%" + nameLike + "%");
    }
    if (startTs > 0) {
        sql += " AND start_ts_ms >= ?";
        intParams.push_back(startTs);
    }
    if (endTs > 0) {
        sql += " AND start_ts_ms <= ?";
        intParams.push_back(endTs);
    }
    sql += " ORDER BY start_ts_ms DESC";
    if (limit > 0) {
        sql += " LIMIT " + std::to_string(limit);
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        Logger::error("dag_run query prepare failed: " + Db::instance().last_error());
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
        DagRunRow r;
        r.runId        = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        r.name         = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        r.source       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        r.status       = sqlite3_column_int(stmt, 3);
        r.startTsMs    = sqlite3_column_int64(stmt, 4);
        r.endTsMs      = sqlite3_column_int64(stmt, 5);
        r.total        = sqlite3_column_int(stmt, 6);
        r.successCount = sqlite3_column_int(stmt, 7);
        r.failedCount  = sqlite3_column_int(stmt, 8);
        r.skippedCount = sqlite3_column_int(stmt, 9);
        const unsigned char* msg = sqlite3_column_text(stmt, 10);
        r.message = msg ? reinterpret_cast<const char*>(msg) : "";
        r.dagJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
        r.workflowJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));
        rows.push_back(std::move(r));
    }

    sqlite3_finalize(stmt);
    return rows;
}

} // namespace taskhub
