#include "http_strategy.h"
#include "httplib.h"
namespace taskhub::runner {
    /// 执行HTTP调用任务
    /// @param cfg 任务配置信息，包含URL和其他执行参数
    /// @param cancelFlag 取消标志，用于检查任务是否被取消
    /// @param deadline 任务截止时间点，用于超时控制
    /// @return 返回任务执行结果，包括执行状态、消息和执行时长
    core::TaskResult HttpExecutionStrategy::execute(const core::TaskConfig &cfg, std::atomic_bool *cancelFlag, Deadline deadline)
    {
        core::TaskResult r;
        const auto start = SteadyClock::now();
        if (cfg.execCommand.empty()) {
            r.status  = core::TaskStatus::Failed;
            r.message = "TaskRunner：空的http url";
            return r;
        }
    
        if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
            r.status  = core::TaskStatus::Canceled;
            r.message = "TaskRunner：在http调用之前取消";
            return r;
        }

        // 粗略解析 URL，支持 http://host[:port]/path 形式
        std::string scheme = "http";
        std::string host;
        std::string path = "/";
        int port         = 80;

        const auto schemePos = cfg.execCommand.find("://");
        std::string rest     = cfg.execCommand;
        if (schemePos != std::string::npos) {
            scheme = cfg.execCommand.substr(0, schemePos);
            rest   = cfg.execCommand.substr(schemePos + 3);
        }

        const auto pathPos = rest.find('/');
        const std::string hostPort = pathPos == std::string::npos ? rest : rest.substr(0, pathPos);
        if (pathPos != std::string::npos) {
            path = rest.substr(pathPos);
        }
    
        const auto colonPos = hostPort.find(':');
        if (colonPos != std::string::npos) {
            host = hostPort.substr(0, colonPos);
            port = std::atoi(hostPort.substr(colonPos + 1).c_str());
        } else {
            host = hostPort;
            port = scheme == "https" ? 443 : 80;
        }
    
        if (scheme != "http" || host.empty()) {
            r.status  = core::TaskStatus::Failed;
            r.message = "TaskRunner：不受支持或无效的URL";
            return r;
        }

        httplib::Client cli(host, port);
        cli.set_keep_alive(false);
    
        if (deadline != SteadyClock::time_point::max()) {
            const auto now = SteadyClock::now();
            if (deadline <= now) {
                r.status  = core::TaskStatus::Timeout;
                r.message = "TaskRunner: timeout before http call";
                return r;
            }
            const auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
            const auto sec    = static_cast<int>(remain.count() / 1000);
            const auto usec   = static_cast<int>((remain.count() % 1000) * 1000);
            cli.set_connection_timeout(sec, usec);
            cli.set_read_timeout(sec, usec);
            cli.set_write_timeout(sec, usec);
        }

        httplib::Params params;
        for (const auto &kv : cfg.execParams) {
            params.emplace(kv.first, kv.second);
        }

        auto res = params.empty() ? cli.Get(path) : cli.Post(path, params);

        if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
            r.status  = core::TaskStatus::Canceled;
            r.message = "TaskRunner：在http调用期间取消";
        } else if (!res) {
            r.status  = core::TaskStatus::Failed;
            r.message = "TaskRunner: http 错误 " + httplib::to_string(res.error());
        } else if (res->status >= 200 && res->status < 300) {
            r.status = core::TaskStatus::Success;
        } else {
            r.status  = core::TaskStatus::Failed;
            r.message = "TaskRunner: http 状态 " + std::to_string(res->status);
        }

        Logger::info("HttpExecutionStrategy::execute, id=" + cfg.id.value +
                    ", name=" + cfg.name +
                    ", status=" + std::to_string(static_cast<int>(r.status)) +
                    ", message=" + r.message);
        r.duration = std::chrono::duration_cast<std::chrono::milliseconds>(SteadyClock::now() - start);
        return r;
    }
}