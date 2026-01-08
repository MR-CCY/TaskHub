// log_sink_file.cpp
#include "log_sink_file.h"
#include "log_rotation.h"
#include "log_formatter.h"
#include <filesystem>

namespace fs = std::filesystem;
namespace taskhub::core {

FileLogSink::FileLogSink(Options opt) : _opt(std::move(opt)) {

    RotationPolicy p;
    p.maxBytes = _opt.rotateBytes;
    p.maxFiles = _opt.maxFiles;
    p.compress = false; // TODO later
    _rot=std::make_unique<LogRotation>(p);
}

void FileLogSink::ensureOpen_() {
    if (_ofs.is_open()) return;
    if (_opt.path.empty()) return;

    auto parent = fs::path(_opt.path).parent_path();
    if (!parent.empty() && !fs::exists(parent)) {
        fs::create_directories(parent);
    }
    _ofs.open(_opt.path, std::ios::app);
}

void FileLogSink::rotateIfNeeded_(std::uint64_t addBytes) {
    if (_opt.rotateBytes == 0) return;

    std::error_code ec;
    auto current = fs::file_size(_opt.path, ec);
    if (ec) return;

    if (_rot->shouldRotate(
            _opt.path,
            static_cast<std::uint64_t>(current),
            addBytes)) {
        doRotate_();
    }
}

void FileLogSink::doRotate_() {
    if (_ofs.is_open()) _ofs.close();
    _rot->rotate(_opt.path);

    // Re-open the new current log file
    _ofs.open(_opt.path, std::ios::app);
}

void FileLogSink::consume(const LogRecord& rec) {
    std::lock_guard<std::mutex> lk(_mu);
    ensureOpen_();
    // 1. 用 formatter 生成“一行文本”
    const std::string line =LogFormatter::instance().formatLine(rec);

    // 2. 在写入前判断是否需要轮转
    rotateIfNeeded_(static_cast<std::uint64_t>(line.size() + 1)); // + '\n'

    // 3. 写入
    _ofs << line << "\n";

    if (_opt.flushEachLine) {
        _ofs.flush();
    }
}

}
