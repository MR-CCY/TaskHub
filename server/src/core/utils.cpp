#include "utils.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace taskhub {
namespace utils {

std::string now_string() {
    using namespace std::chrono;

    // 当前时间点（系统时间）
    auto now = system_clock::now();

    // 拆成秒 + 毫秒
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t t = system_clock::to_time_t(now);

    // 转换成本地时间（线程安全）
    std::tm buf {};
#if defined(_WIN32)
    localtime_s(&buf, &t);
#else
    localtime_r(&t, &buf);
#endif

    std::ostringstream oss;
    oss << std::put_time(&buf, "%Y-%m-%d %H:%M:%S")
        << "." << std::setfill('0') << std::setw(3) << ms.count();

    return oss.str();
}

long long now_millis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
               system_clock::now().time_since_epoch()
           ).count();
}

std::pair<int, std::string> run_command(const std::string &cmd)
{
    std::array<char, 256> buffer{};
    std::string result;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return { -1, "popen() failed" };
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    int rc = pclose(pipe);  // rc 一般是进程返回码 << 8，不用太纠结

    return { rc, result };

}

} // namespace utils
} // namespace taskhub