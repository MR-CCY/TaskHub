#include "log_rotation.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <vector>
#include <algorithm>

#include <filesystem>

namespace fs = std::filesystem;

namespace taskhub::core {

LogRotation::LogRotation(RotationPolicy policy) : _p(std::move(policy)) {}

bool LogRotation::shouldRotate(const std::string& /*basePath*/,
                               std::uint64_t currentSizeBytes,
                               std::uint64_t addBytes) const {
    if (_p.maxBytes == 0) return false;
    return (currentSizeBytes + addBytes) > _p.maxBytes;
}

std::string LogRotation::nowStamp_() {
    using namespace std::chrono;
    auto now = system_clock::now();
    std::time_t tt = system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d-%H%M%S");
    return oss.str();
}

bool LogRotation::exists_(const std::string& path) {
    std::error_code ec;
    return fs::exists(path, ec);
}

std::uint64_t LogRotation::fileSize_(const std::string& path) {
    std::error_code ec;
    auto sz = fs::file_size(path, ec);
    if (ec) return 0;
    return static_cast<std::uint64_t>(sz);
}

void LogRotation::rename_(const std::string& from, const std::string& to) {
    std::error_code ec;
    fs::rename(from, to, ec);
    if (ec) {
        // 如果 rename 失败，尝试 copy + remove（跨盘场景）
        fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
        if (!ec) fs::remove(from, ec);
    }
}

std::string LogRotation::makeRotatedName_(const std::string& basePath, int index) {
    // basePath + "." + stamp + "." + index
    std::ostringstream oss;
    oss << basePath << "." << nowStamp_() << "." << index;
    return oss.str();
}

void LogRotation::prune_(const std::string& basePath) const {
    if (_p.maxFiles <= 0) return;

    std::vector<fs::directory_entry> rotated;
    std::error_code ec;

    fs::path base(basePath);
    fs::path dir = base.parent_path().empty() ? fs::current_path() : base.parent_path();
    std::string filename = base.filename().string(); // "taskhub.log"

    for (auto& e : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!e.is_regular_file()) continue;
        const auto name = e.path().filename().string();
        // 简单匹配：以 "taskhub.log." 开头的都算轮转文件
        if (name.rfind(filename + ".", 0) == 0) {
            rotated.push_back(e);
        }
    }

    // 按最后写入时间排序（旧的在前）
    std::sort(rotated.begin(), rotated.end(),
              [](const fs::directory_entry& a, const fs::directory_entry& b) {
                  std::error_code ec1, ec2;
                  auto ta = fs::last_write_time(a, ec1);
                  auto tb = fs::last_write_time(b, ec2);
                  if (ec1 || ec2) return a.path().string() < b.path().string();
                  return ta < tb;
              });

    // 超过 maxFiles 就删最旧的
    while (static_cast<int>(rotated.size()) > _p.maxFiles) {
        std::error_code rmec;
        fs::remove(rotated.front().path(), rmec);
        rotated.erase(rotated.begin());
    }
}

void LogRotation::rotate(const std::string& basePath) const {
    if (!exists_(basePath)) return;

    // 找一个不冲突的 index
    int idx = 1;
    std::string rotatedPath;
    for (; idx < 10000; ++idx) {
        rotatedPath = makeRotatedName_(basePath, idx);
        if (!exists_(rotatedPath)) break;
    }

    rename_(basePath, rotatedPath);

    // TODO: compress rotatedPath if (_p.compress)

    prune_(basePath);
}

} // namespace taskhub::core