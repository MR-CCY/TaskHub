#pragma once
#include <string>
#include <cstdint>

namespace taskhub::core {

// 文件轮转策略：先实现“按 size 轮转”
// 命名：
//   taskhub.log           （当前写入）
//   taskhub.log.20251214-235959.1  （轮转出来的）
// 也支持保留 N 个历史文件（简单清理）
struct RotationPolicy {
    std::uint64_t maxBytes = 10 * 1024 * 1024; // 10MB
    int maxFiles = 5;                          // 最多保留 5 个轮转文件
    bool compress = false;                     // TODO: 以后可做 .gz
};

class LogRotation {
public:
    explicit LogRotation(RotationPolicy policy);

    // basePath: 例如 "/var/log/taskhub.log" 或 "./taskhub.log"
    // currentSizeBytes: 当前文件大小（写入前）
    // addBytes: 本次将要追加的字节数
    // 返回 true 表示需要轮转
    bool shouldRotate(const std::string& basePath,
                      std::uint64_t currentSizeBytes,
                      std::uint64_t addBytes) const;

    // 执行轮转：basePath -> basePath.<timestamp>.<index>
    // 返回轮转后的新“当前文件路径”（仍然是 basePath）
    void rotate(const std::string& basePath) const;

private:
    RotationPolicy _p;

    static std::string makeRotatedName_(const std::string& basePath, int index);
    static std::string nowStamp_(); // YYYYMMDD-HHMMSS
    static std::uint64_t fileSize_(const std::string& path);
    static bool exists_(const std::string& path);

    // 删除多余历史文件：basePath.*，只保留 maxFiles
    void prune_(const std::string& basePath) const;

    // 简单：重命名
    static void rename_(const std::string& from, const std::string& to);
};

} // namespace taskhub::core