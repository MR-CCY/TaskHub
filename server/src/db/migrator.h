#pragma once

#include <string>
#include <sqlite3.h>

namespace taskhub {

class DbMigrator {
public:
    static constexpr int  PROGRAM_SCHEMA_VERSION =6;
    static constexpr const char* PROGRAM_APP_VERSION="v1.0.5";

    // 入口：启动时调用
    static void migrate(sqlite3* db, const std::string& migrations_dir);

private:
    static int  get_db_schema_version(sqlite3* db);
    static bool table_exists(sqlite3* db, const std::string& table);
    static bool column_exists(sqlite3* db,
                              const std::string& table,
                              const std::string& column);

    static int  infer_schema_version_and_bootstrap_meta(sqlite3* db,
                                                        const std::string& migrations_dir);
    static void apply_migrations(sqlite3* db,
                                 int from_version,
                                 int to_version,
                                 const std::string& migrations_dir);
    static void exec_sql_file(sqlite3* db, const std::string& filepath);
};

} // namespace taskhub
