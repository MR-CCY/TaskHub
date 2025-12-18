#include "migrator.h"
#include "log/logger.h"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace taskhub {
    // constexpr const char* DbMigrator::PROGRAM_APP_VERSION = "v1.0.1";
    void DbMigrator::migrate(sqlite3 *db, const std::string &migrations_dir)
    {
        //TODO:启动时调用的入口
        int db_ver = get_db_schema_version(db);

        if (db_ver == 0) {
            db_ver = infer_schema_version_and_bootstrap_meta(db, migrations_dir);
        }
        if (db_ver < PROGRAM_SCHEMA_VERSION) {
            apply_migrations(db, db_ver, PROGRAM_SCHEMA_VERSION, migrations_dir);
        } else if (db_ver > PROGRAM_SCHEMA_VERSION) {
            // 打 warn，提示当前程序版本比 DB 老
            Logger::error("提示当前程序版本比 DB 老");
        } else {
            // 已是最新
        }
    }

    int DbMigrator::get_db_schema_version(sqlite3 *db)
    {
        const char* sql =
        "SELECT schema_version FROM taskhub_meta WHERE id = 1;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            // 可能是 meta 表不存在
            return 0;
        }

        int version = 0;
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            version = sqlite3_column_int(stmt, 0);
        } else {
            version = 0;
        }
        sqlite3_finalize(stmt);
        return version;
    }
    bool DbMigrator::table_exists(sqlite3 *db, const std::string &table)
    {
        const char* sql =
        "SELECT name FROM sqlite_master WHERE type='table' AND name=?;";

        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, table.c_str(), -1, SQLITE_TRANSIENT);

        bool exists = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            exists = true;
        }
        sqlite3_finalize(stmt);
        return exists;
    }
    bool DbMigrator::column_exists(sqlite3 *db, const std::string &table, const std::string &column)
    {
        
        std::string sql = "PRAGMA table_info(" + table + ");";
        // PRAGMA table_info(table_name);
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    
        bool found = false;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* col_name = sqlite3_column_text(stmt, 1);
            if (col_name && column == reinterpret_cast<const char*>(col_name)) {
                found = true;
                break;
            }
        }
        sqlite3_finalize(stmt);
        return found;
    }
    int DbMigrator::infer_schema_version_and_bootstrap_meta(sqlite3 *db, const std::string &migrations_dir)
    {
           /**
         * @brief 推断数据库当前的模式版本并初始化元数据表
         * 
         * 该函数通过检查特定表和列的存在性来推断数据库的当前模式版本。
         * 如果是新数据库（tasks表不存在），则执行初始迁移脚本。
         * 同时确保taskhub_meta元数据表存在，并记录当前的模式版本信息。
         * 
         * @param db SQLite数据库连接指针
         * @param migrations_dir 数据库迁移脚本所在的目录路径
         * @return 推断出的模式版本号（1表示v1.0.0，2表示v1.0.1）
         */
        // 检查是否为新数据库，如果是则执行初始迁移
        if (!table_exists(db, "tasks")) {
            exec_sql_file(db, migrations_dir + "/001_v1_0_0_init.sql");
            return 1;
        }

        // 通过检查v1.0.1版本新增的列来推断当前模式版本
        const bool has_v101_columns =
            column_exists(db, "tasks", "timeout_sec") &&
            column_exists(db, "tasks", "max_retries") &&
            column_exists(db, "tasks", "retry_count") &&
            column_exists(db, "tasks", "cancel_flag");

        const int inferred_version = has_v101_columns ? 2 : 1;

        // 确保元数据表存在，如果不存在则创建并插入初始记录
        if (!table_exists(db, "taskhub_meta")) {
            const char* create_meta_sql =
                "CREATE TABLE IF NOT EXISTS taskhub_meta ("
                "id INTEGER PRIMARY KEY,"
                "schema_version INTEGER NOT NULL,"
                "app_version TEXT NOT NULL,"
                "upgrade_time TEXT NOT NULL"
                ");";
            sqlite3_exec(db, create_meta_sql, nullptr, nullptr, nullptr);

            const char* app_version = inferred_version == 2 ? "v1.0.1" : "v1.0.0";
            std::string insert_sql =
                "INSERT OR REPLACE INTO taskhub_meta "
                "(id, schema_version, app_version, upgrade_time) VALUES (1, " +
                std::to_string(inferred_version) + ", '" + app_version +
                "', datetime('now'));";
            sqlite3_exec(db, insert_sql.c_str(), nullptr, nullptr, nullptr);
        }

        return inferred_version;
    }
    void DbMigrator::apply_migrations(sqlite3 *db, int from_version, int to_version, const std::string &migrations_dir)
    {
        for (int v = from_version + 1; v <= to_version; ++v) {
            if (v == 2) {
                exec_sql_file(db, migrations_dir + "/002_v1_0_1_add_task_fields.sql");
            }
            else if(v==3){
                exec_sql_file(db, migrations_dir + "/003_v1_0_2_add_template.sql");

            }else {
                Logger::error("未知的迁移版本: " + std::to_string(v));
                break;
            }

            const int current_version = get_db_schema_version(db);
            if (current_version != v) {
                Logger::error("迁移后 schema_version 未更新到 " + std::to_string(v));
                break;
            }
        }
    }
    void DbMigrator::exec_sql_file(sqlite3 *db, const std::string &filepath)
    {
        namespace fs = std::filesystem;
        Logger::info("尝试加载迁移文件: " + filepath + ", cwd=" + fs::current_path().string());
        if (!fs::exists(filepath)) {
            Logger::error("迁移文件不存在: " + filepath);
        }
        std::ifstream file(filepath);
        if (!file.is_open()) {
            Logger::error("无法打开 SQL 文件: " + filepath);
            return;
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();
        const std::string sql = buffer.str();

        char* err_msg = nullptr;
        int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err_msg);
        if (rc != SQLITE_OK) {
            Logger::error("执行 SQL 文件失败: " + filepath + "，错误: " + (err_msg ? err_msg : ""));
            sqlite3_free(err_msg);
        }
    }
}
