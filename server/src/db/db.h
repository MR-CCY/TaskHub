#pragma once

#include <string>
#include <functional>
#include <sqlite3.h>

namespace taskhub {

class Db {
public:
    static Db& instance();

    // 打开数据库：path 由 Config 决定
    bool open(const std::string& path);
    void close();

    // 执行无结果的 SQL（建表、插入、更新等）
    bool exec(const std::string& sql);

    // 简单封装 prepare，用于查询
    sqlite3* handle() const { return m_db; }

    std::string last_error() const;

private:
    Db() = default;
    ~Db();

    Db(const Db&) = delete;
    Db& operator=(const Db&) = delete;

private:
    sqlite3* m_db = nullptr;
};

} // namespace taskhub