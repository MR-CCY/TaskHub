// log_sink_file.h
#pragma once
#include "log_sink.h"
#include <fstream>
#include <string>
#include "core/config.h"
#include <mutex>
#include <memory>

namespace taskhub::core {
class LogRotation;
class FileLogSink : public ILogSink {
public:
    struct Options {
        std::string path = "./logs/taskhub.log";
        std::size_t rotateBytes = 10 * 1024 * 1024; // 10MB
        int maxFiles = 5; // taskhub.log.1 ... .5
        bool flushEachLine = false;
    };

    explicit FileLogSink(Options opt);
    void consume(const LogRecord& rec) override;

private:
    void ensureOpen_();
    void rotateIfNeeded_(std::uint64_t addBytes);
    void doRotate_();

private:
    Options _opt;
    std::ofstream _ofs;
    std::unique_ptr<LogRotation> _rot;
    std::mutex _mu;

};

}
