#pragma once
#include "task_common.h"
#include <string>
#include <chrono>
#include <unordered_map>   // ✅补上
#include "json.hpp"
using json = nlohmann::json;
namespace taskhub::core {

struct TaskResult {
    TaskStatus status{ TaskStatus::Pending };
    std::string message;

    // ✅统一：用毫秒
    //持续时间毫秒
    long long durationMs{0};

    // ✅统一：stdout/stderr
    //标准输出
    std::string stdoutData;
    //标准错误
    std::string stderrData;

    int exitCode{0};

    int attempt{1};
    int maxAttempts{1};

    std::string workerId;
    std::string workerHost;
    int workerPort{0};

    std::unordered_map<std::string, std::string> metadata;

    bool ok() const { return status == TaskStatus::Success; }
};

inline TaskResult parseResultJson(const json& j)
{
    TaskResult r;

    // 允许两种返回：直接平铺 or { "result": {...} }
    const json* p = &j;
    if (j.contains("result") && j["result"].is_object()) {
        p = &j["result"];
    }
    const json& jr = *p;

    // status: int (推荐)
    // 默认 Failed(3) 以免 worker 返回缺字段时误判成功
    int statusInt = jr.value("status", 3);
    r.status = static_cast<taskhub::core::TaskStatus>(statusInt);

    r.message    = jr.value("message", std::string{});
    r.exitCode   = jr.value("exit_code", 0);
    r.durationMs = jr.value("duration_ms", 0LL);

    r.stdoutData = jr.value("stdout", std::string{});
    r.stderrData = jr.value("stderr", std::string{});

    r.attempt     = jr.value("attempt", 1);
    r.maxAttempts = jr.value("max_attempts", 1);

    // metadata (可选)
    if (jr.contains("metadata") && jr["metadata"].is_object()) {
        for (auto it = jr["metadata"].begin(); it != jr["metadata"].end(); ++it) {
            if (it.value().is_string()) {
                r.metadata[it.key()] = it.value().get<std::string>();
            } else {
                // 非字符串也转成 dump，避免丢信息
                r.metadata[it.key()] = it.value().dump();
            }
        }
    }

    return r;
}
inline json taskResultToJson(const core::TaskResult& r) {
    json j;
    j["status"]      = static_cast<int>(r.status);   // ✅ int enum
    j["message"]     = r.message;
    j["exit_code"]   = r.exitCode;
    j["duration_ms"] = r.durationMs;
    j["stdout"]      = r.stdoutData;
    j["stderr"]      = r.stderrData;
    j["attempt"]     = r.attempt;
    j["max_attempts"]= r.maxAttempts;

    // metadata 可选
    json jm = json::object();
    for (const auto& kv : r.metadata) jm[kv.first] = kv.second;
    j["metadata"] = jm;

    return j;
}

} // namespace taskhub::core