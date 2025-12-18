#include "cron_scheduler.h"
#include "log/logger.h"
#include "runner/taskRunner.h"
#include "dag/dag_service.h"
namespace taskhub::scheduler {
    CronScheduler &CronScheduler::instance()
    {
        // TODO: 在此处插入 return 语句
        static CronScheduler instance;
        return instance;
    }

    void CronScheduler::start()
    {
        bool expected = false;
        if (!_started && _worker.joinable() == false) {
            _stopping = false;
            _worker   = std::thread(&CronScheduler::loop, this);
            _started  = true;
            Logger::info("CronScheduler started");
        }
    }
    void CronScheduler::stop()
    {
        if (!_started) return;

        _stopping = true;
        _cv.notify_all();

        if (_worker.joinable()) {
            _worker.join();
        }
        _started = false;
        Logger::info("CronScheduler stopped");
    }
    std::string CronScheduler::addJob(const CronJob &job)
    {
        std::lock_guard<std::mutex> lk(_mutex);

        // TODO：如果 job.id 为空，可以在这里生成一个 ID（简单的话用自增或时间戳）
        std::string id = job.id.empty() ? job.id : job.id;
    
        _jobs.push_back(job);
        // 保证 vector 里存的 id 是最终 id
        if (id.empty()) {
            _jobs.back().id = "job_" + std::to_string(_jobs.size());
            id = _jobs.back().id;
        }
    
        Logger::info("CronScheduler::addJob, id=" + id +
                     ", name=" + job.name +
                     ", spec=" + job.spec);
    
        // 有新 Job，唤醒 loop 重新计算最近触发时间
        _cv.notify_all();
        return id;
    }
    void CronScheduler::removeJob(const std::string &jobId)
    {
        std::lock_guard<std::mutex> lk(_mutex);

        auto it = std::remove_if(_jobs.begin(), _jobs.end(),
                                [&jobId](const CronJob& j) {
                                    return j.id == jobId;
                                });
        if (it != _jobs.end()) {
            _jobs.erase(it, _jobs.end());
            Logger::info("CronScheduler::removeJob, id=" + jobId);
        }

        _cv.notify_all();
    }
    std::vector<CronJob> CronScheduler::listJobs() const
    {
        std::lock_guard<std::mutex> lk(_mutex);
        return _jobs; 
    }
    CronScheduler::~CronScheduler()
    {
        stop();
    }
    void CronScheduler::loop()
    {
        while (!_stopping.load(std::memory_order_acquire)) {
            std::unique_lock<std::mutex> lk(_mutex);
            auto now = std::chrono::system_clock::now();
    
            ScanResult scan = scanJobsLocked(now);
    
            if (!scan.hasJob) {
                // 当前没有任何 job，等待被 addJob / stop 唤醒
                _cv.wait(lk, [this] {
                    return _stopping.load(std::memory_order_acquire) || !_jobs.empty();
                });
                continue;
            }
    
            if (!scan.pendingIndices.empty()) {
                // 有“已经到点/过期”的 job 需要立刻触发
                auto pending = scan.pendingIndices;
    
                // 解锁再实际触发，避免持锁执行逻辑
                lk.unlock();
    
                for (auto idx : pending) {
                    CronJob jobCopy = [&]() {
                        std::lock_guard<std::mutex> lk2(_mutex);
                        if (idx >= _jobs.size()) {
                            // 返回一个“占位”的 CronJob，不触发就行
                            return _jobs.front(); // 理论上不会走到这里，只是为了编译通过
                        }
                
                        // 复制一份当前的 Job
                        CronJob copy = _jobs[idx];
                
                        // 更新下一次触发时间（作用在 _jobs[idx] 上）
                        auto now2 = std::chrono::system_clock::now();
                        _jobs[idx].nextTime = _jobs[idx].cronExpr.next(now2);
                
                        return copy;
                    }();
    
                    // TODO(M10.3)：这里真正触发任务执行
                    // 现在先打日志占位
                    Logger::info("CronScheduler::trigger job, id=" + jobCopy.id +
                                 ", name=" + jobCopy.name +
                                 ", spec=" + jobCopy.spec);

                                // ===== 真正执行逻辑（同步调用） =====
                    try {
                        if (jobCopy.targetType == CronTargetType::SingleTask) {
                            auto& runner=runner::TaskRunner::instance();
                            if (!jobCopy.taskTemplate.has_value()) {
                                Logger::error("CronScheduler: taskTemplate not set for SingleTask job id=" +
                                    jobCopy.id);
                            } else {
                                core::TaskConfig cfg = *jobCopy.taskTemplate;

                                // 给这个任务打个 “cron 前缀”，方便在日志/DB 中区分
                                cfg.id.value = "cron_" + jobCopy.id + "_" + cfg.id.value;

                                Logger::info("CronScheduler: execute SingleTask job, taskId=" + cfg.id.value);
                                core::TaskResult r = runner.run(cfg, nullptr);

                                Logger::info("CronScheduler: SingleTask result, taskId=" + cfg.id.value +
                                            ", status=" + std::to_string(static_cast<int>(r.status)) +
                                            ", message=" + r.message);
                            }
                        } else if (jobCopy.targetType == CronTargetType::Dag) {
                            //todo:返回值处理
                            auto& dagService=dag::DagService::instance();

                            if (jobCopy.dagPayload.has_value()) {
                                auto& d = *jobCopy.dagPayload;
                                core::TaskResult r = dagService.runDag(d.specs, d.config, d.callbacks);
                                Logger::info("CronScheduler: Dag job result, id=" + jobCopy.id +
                                                ", status=" + std::to_string(static_cast<int>(r.status)) +
                                                ", message=" + r.message);
                            }
                           
                        } else {
                            Logger::warn("CronScheduler: Unknown targetType for job id=" + jobCopy.id);
                        }
                    } catch (const std::exception& ex) {
                        Logger::error(std::string("CronScheduler: exception when executing job id=")
                                    + jobCopy.id + ", what=" + ex.what());
                    }                
                }
    
                // 回到 while 顶部，再次 scan
                continue;
            }
    
            // 没有 pending 的 job，但有未来的 nextTime → 等到 nearestNext 或被唤醒
            auto waitDuration = scan.nearestNext - now;
            if (waitDuration <= std::chrono::seconds(0)) {
                // 理论上不会走到这里，保险起见
                waitDuration = std::chrono::seconds(1);
            }
    
            _cv.wait_for(lk, waitDuration, [this] {
                return _stopping.load(std::memory_order_acquire);
            });
        }
    }
    CronScheduler::ScanResult CronScheduler::scanJobsLocked(std::chrono::system_clock::time_point now) const
    {
        ScanResult res;

        if (_jobs.empty()) {
            res.hasJob = false;
            return res;
        }
    
        res.hasJob = true;
        bool firstNextInit = false;
    
        for (std::size_t i = 0; i < _jobs.size(); ++i) {
            const auto& job = _jobs[i];
            if (!job.enabled) {
                continue;
            }
    
            // 如果 nextTime 已经过了当前时间，认为该 job 需要立刻触发
            if (job.nextTime <= now) {
                res.pendingIndices.push_back(i);
            }
    
            if (!firstNextInit) {
                res.nearestNext = job.nextTime;
                firstNextInit   = true;
            } else {
                if (job.nextTime < res.nearestNext) {
                    res.nearestNext = job.nextTime;
                }
            }
        }
    
        return res;
    }
}