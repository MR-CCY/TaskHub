#include "utils_exec.h"
#include <future>
#include <chrono>
#include <utility>
#include "core/utils.h"

namespace taskhub {
    ExecResult run_with_timeout(const std::string &cmd, int timeout_sec)
    {
        ExecResult result;

        auto fut = std::async(std::launch::async, [cmd]() {
            ExecResult inner;
            try {
                auto [code, out] = utils::run_command(cmd);
                inner.exit_code = code;
                inner.output = std::move(out);
            } catch (const std::exception &ex) {
                inner.exit_code = -1;
                inner.error = ex.what();
            }
            return inner;
        });
    
        if (timeout_sec > 0) {
            auto status = fut.wait_for(std::chrono::seconds(timeout_sec));
            if (status == std::future_status::ready) {
                result = fut.get();
            } else {
                result.timed_out = true;
                // 注意：这里 demo 不强杀子进程，只是逻辑上认为超时
                result.exit_code = -1;
                result.error = "Task timeout after " + std::to_string(timeout_sec) + " seconds";
            }
        } else {
            // 不限制时间
            result = fut.get();
        }

        return result;
    } 
}
