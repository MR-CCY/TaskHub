#pragma once
#include <string>
#include <variant>
#include <map>
#include <vector>
namespace taskhub {
    using ConfigVariant = std::variant<int, std::string,bool,double,float,long,std::vector<std::string>>;
class Config {
public:
    // 单例接口
    static Config& instance();

    // 加载配置文件
    bool load(const std::string& path);

    // 从环境变量覆盖配置
    void load_from_env();

    // 配置访问接口
    std::string host() const{ return m_host; };
    int port() const{ return m_port; };
    std::string db_host() const{ return m_db_host; };
    int db_port() const{    return m_db_port; };
    std::string db_path() const{ return m_db_path; };
    std::string log_path() const{ return m_log_path; };
     // 设置不同类型的配置值
     void set(const std::string& key, bool value) {
        m_config[key] = value;
    }
    
    void set(const std::string& key, int value) {
        m_config[key] = value;
    }
    
    void set(const std::string& key, const std::string& value) {
        m_config[key] = value;
    }
    void set(const std::string& key, double value) {
        m_config[key] = value;
    }
    void set(const std::string& key, float value) {
        m_config[key] = value;
    }
    void set(const std::string& key, long value) {
        m_config[key] = value;
    }
    void set(const std::string& key, const std::vector<std::string>& value) {
        m_config[key] = value;
    }
    
    // 获取不同类型配置值的方法
    template<typename T>
    T get(const std::string& key, const T& default_value = T{}) const {
        auto it = m_config.find(key);
        if (it != m_config.end()) {
            try {
                return std::get<T>(it->second);
            } catch (const std::bad_variant_access&) {
                // 类型不匹配时返回默认值
            }
        }
        return default_value;
    }
private:
    Config();                           // 私有构造
    Config(const Config&) = delete;     // 禁止拷贝
    Config& operator=(const Config&) = delete;

private:
    std::string m_host = "0.0.0.0";
    int m_port = 8080;
    int m_db_port = 8082;
    std::string m_db_host = "0.0.0.0";
    std::string m_db_path = "taskhub.db";
    std::string m_log_path = "./logs";
    std::map<std::string, ConfigVariant> m_config;
};

} // namespace taskhub
