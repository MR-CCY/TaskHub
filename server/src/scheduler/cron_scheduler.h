#pragma once

#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "scheduler/cron_job.h"

namespace taskhub {
    namespace runner { class TaskRunner; }
    namespace dag    { class DagService; }
    }
namespace taskhub::scheduler {

/// CronScheduler：维护所有 CronJob，并在后台线程里按时触发
class CronScheduler {
public:
    static CronScheduler& instance();

    /// 启动调度线程（Server 启动时调用一次）
    void start();

    /// 停止调度线程（Server 退出时调用）
    void stop();

    /// 新增一个 Job，返回 job.id（如果 job.id 为空，可以在内部生成）
    std::string addJob(const CronJob& job);

    /// 删除一个 Job（如果不存在则忽略）
    void removeJob(const std::string& jobId);

    /// 获取当前所有 Job（供管理接口查询使用）
    std::vector<CronJob> listJobs() const;

private:
    CronScheduler() = default;
    ~CronScheduler();

    CronScheduler(const CronScheduler&) = delete;
    CronScheduler& operator=(const CronScheduler&) = delete;

    /// 后台主循环
    void loop();

    /// 找出最近一次要触发的时间，以及所有应该触发的 job 下标
    ///
    /// 返回值：
    ///  - hasJob = false：当前没有任何 job
    ///  - hasJob = true 且 pendingIndices 非空：表示有“已经过期或正好到点”的 job 需要立刻触发
    ///  - hasJob = true 且 pendingIndices 为空：表示未来有 job，要等到 nearestNext 再触发
    struct ScanResult {
        bool hasJob{false};
        std::chrono::system_clock::time_point nearestNext{};
        std::vector<std::size_t> pendingIndices;
    };

    ScanResult scanJobsLocked(std::chrono::system_clock::time_point now) const;

private:
    mutable std::mutex          _mutex;
    std::condition_variable     _cv;
    std::vector<CronJob>        _jobs;

    std::thread                 _worker;
    std::atomic_bool            _stopping{false};
    bool                        _started{false};

    dag::DagService*    _dagService{nullptr};
};

} // namespace taskhub::scheduler