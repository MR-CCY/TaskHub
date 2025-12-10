#include "cron_job.h"
namespace taskhub::scheduler {
    CronJob::CronJob(const std::string &jobId, const std::string &jobName, const std::string &cronSpec, CronTargetType type) 
    : id(jobId)
    , name(jobName)
    , spec(cronSpec)
    , targetType(type)
    , cronExpr(cronSpec)
    {
        // 默认：构造时就算一次 nextTime
        auto now = std::chrono::system_clock::now();
        nextTime = cronExpr.next(now);
    }
}