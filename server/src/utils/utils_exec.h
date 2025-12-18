// utils/utils_exec.h
#pragma once
#include <string>

namespace taskhub {

struct ExecResult {
    int exit_code{-1};
    std::string output;
    std::string error;
    bool timed_out = false;
};

ExecResult run_with_timeout(const std::string& cmd, int timeout_sec);

} // namespace taskhub
