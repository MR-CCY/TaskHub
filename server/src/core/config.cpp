#include "config.h"
#include <cstdlib>          // getenv
#include "json.hpp"         // 你之后要 include
#include "logger.h"
#include <fstream>
namespace taskhub {

Config& Config::instance() {
    static Config instance;
    return instance;
}

Config::Config() {
    // 可以放默认值
}

// 加载 JSON 配置文件
bool Config::load(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        Logger::warn("Config file not found: " + path + ", using defaults");
        return false;
    }

    try {
        nlohmann::json j;
        ifs >> j;
        for (auto& [key, value] : j.items()) {
            if (value.is_object()) {
                m_config[key] = value.dump();
                for (auto& [k, v] : value.items()) {
                    if (v.is_boolean()) {
                        m_config[key + "." + k] = v.get<bool>();
                    } else if (v.is_number_integer()) {
                        m_config[key + "." + k] = v.get<int>();
                    } else if (v.is_number_float()) {
                        m_config[key + "." + k] = v.get<double>();
                    } else if (v.is_string()) {
                        m_config[key + "." + k] = v.get<std::string>();
                    }else if(v.is_array()){
                        std::vector<std::string> vec;
                        for(const auto& item : v){
                            if(item.is_string()){
                                vec.push_back(item.get<std::string>());
                            }
                        }
                        m_config[key + "." + k] = vec;
                    }
                }
            }
        }

        // server 部分
        if (j.contains("server") && j["server"].is_object()) {
            const auto& s = j["server"];

            if (s.contains("host") && s["host"].is_string()) {
                m_host = s["host"].get<std::string>();
            }

            if (s.contains("port") && s["port"].is_number_integer()) {
                m_port = s["port"].get<int>();
            }
        }

        // database 部分
        if (j.contains("database") && j["database"].is_object()) {
            const auto& d = j["database"];
            if (d.contains("db_path") && d["db_path"].is_string()) {
                m_db_path = d["db_path"].get<std::string>();
            }
            if (d.contains("host") && d["host"].is_string()) {
                m_db_host = d["host"].get<std::string>();
            }
            if (d.contains("port") && d["port"].is_number_integer()) {
                m_db_port = d["port"].get<int>();
            }
            // if(d.contains("db_log_path") && d["db_log_path"].is_string()) {
            //     m_db_log_path = d["db_log_path"].get<std::string>();
            // }
        }

        // log 部分
        if (j.contains("log") && j["log"].is_object()) {
            const auto& l = j["log"];
            if (l.contains("path") && l["path"].is_string()) {
                m_log_path = l["path"].get<std::string>();
            }
        }
        Logger::info("Config loaded from: " + path);
        return true;
    }
    catch (const std::exception& ex) {
        Logger::error(std::string("Failed to parse config file: ") + path +
                      ", error: " + ex.what());
        return false;
    }
}

// 从环境变量覆盖
void Config::load_from_env() {
    if (const char* p = std::getenv("TASKHUB_PORT")) {
        m_port = std::atoi(p);
    }
    if (const char* p = std::getenv("TASKHUB_HOST")) {
        m_host = p;
    }
    if (const char* p = std::getenv("TASKHUB_DB")) {
        m_db_path = p;
    }
    if (const char* p = std::getenv("TASKHUB_LOG")) {
        m_log_path = p;
    }
}


} // namespace taskhub