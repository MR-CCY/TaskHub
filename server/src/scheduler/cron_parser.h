#pragma once
#include <string>
#include <vector>
#include <chrono>

namespace taskhub::scheduler {

class CronExpr
{
public:
    explicit CronExpr(const std::string& spec);

    // 返回 now 之后的下一次执行时间点
    std::chrono::system_clock::time_point next(const std::chrono::system_clock::time_point& now) const;

private:
    // 解析后的字段值（分钟/小时/天/月/星期）
    std::vector<int> minutes_;
    std::vector<int> hours_;
    std::vector<int> days_;
    std::vector<int> months_;
    std::vector<int> weekdays_;

private:
    void parse(const std::string& spec);

    std::vector<int> parseField(const std::string& field,
                                int min, int max) const;

    // * 1-5 */5 这种处理
    std::vector<int> parseRange(const std::string& token,
                                int min, int max) const;
};

} // namespace taskhub::scheduler