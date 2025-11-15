#pragma once
#include <string>

namespace taskhub {

class Config {
public:
    // 单例接口
    static Config& instance();

    // 加载配置文件
    bool load(const std::string& path);

    // 从环境变量覆盖配置
    void load_from_env();

    // 配置访问接口
    std::string host() const;
    int port() const;
    std::string db_path() const;
    std::string log_path() const;

private:
    Config();                           // 私有构造
    Config(const Config&) = delete;     // 禁止拷贝
    Config& operator=(const Config&) = delete;

private:
    std::string m_host = "0.0.0.0";
    int m_port = 8080;

    std::string m_db_path = "taskhub.db";
    std::string m_log_path = "./logs";
};

} // namespace taskhub