#include "db.h"
#include "core/logger.h"
namespace taskhub {
    Db &taskhub::Db::instance()
    {
        // TODO: 在此处插入 return 语句
        static Db instance;
        return instance;
    }

    bool taskhub::Db::open(const std::string &path)
    {
        if (m_db) {
            return true; // 已经打开
        }
        int rc = sqlite3_open(path.c_str(), &m_db);
        if (rc != SQLITE_OK) {
            Logger::error("Failed to open DB: " + std::string(sqlite3_errmsg(m_db)));
            sqlite3_close(m_db);
            m_db = nullptr;
            return false;
        }
        Logger::info("DB opened at " + path);
        return true;
    }

    void taskhub::Db::close()
    {
        if (m_db) {
            sqlite3_close(m_db);
            m_db = nullptr;
            Logger::info("DB closed");
        }
    }

    bool taskhub::Db::exec(const std::string &sql)
    {
        char* errmsg = nullptr;
        int rc = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::string msg = errmsg ? errmsg : "";
            Logger::error("DB exec error: " + msg + " SQL: " + sql);
            if (errmsg) sqlite3_free(errmsg);
            return false;
        }
        return true;
    }

    std::string taskhub::Db::last_error() const
    {
        if (!m_db) return {};
        return sqlite3_errmsg(m_db);
    }

    taskhub::Db::~Db()
    {
        close();
    }
}  // namespace taskhub