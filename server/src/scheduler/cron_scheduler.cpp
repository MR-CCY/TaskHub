#include "cron_scheduler.h"
#include "log/logger.h"
#include "runner/task_runner.h"
#include "runner/task_config.h"
#include "dag/dag_service.h"
#include "dag/dag_run_utils.h"
#include "template/template_store.h"
#include "template/template_renderer.h"
#include "utils/utils.h"
#include "json.hpp"
#include <utility>
namespace taskhub::scheduler {
    namespace {
        using json = nlohmann::json;

        bool dispatch_single_task(const CronJob& job, const std::string& runId)
        {
            if (!job.taskTemplate.has_value()) {
                Logger::error("CronScheduler: taskTemplate not set for SingleTask job id=" +
                    job.id);
                return false;
            }
            auto& runner = runner::TaskRunner::instance();
            core::TaskConfig cfg = *job.taskTemplate;
            cfg.id.runId = runId;

            Logger::info("CronScheduler: execute SingleTask job, taskId=" + cfg.id.value);
            core::TaskResult r = runner.run(cfg, nullptr);

            Logger::info("CronScheduler: SingleTask result, taskId=" + cfg.id.value +
                        ", status=" + std::to_string(static_cast<int>(r.status)) +
                        ", message=" + r.message);
            return r.ok();
        }

        bool dispatch_dag_job(const CronJob& job, const std::string& runId)
        {
            if (!job.dagPayload.has_value()) {
                Logger::error("CronScheduler: dagPayload not set for Dag job id=" + job.id);
                return false;
            }

            auto d = *job.dagPayload; // copy to mutate runId

            json dagBody;
            dagBody["name"] = job.name;
            json cfgJson = json::object();
            cfgJson["fail_policy"] = d.config.failPolicy == dag::FailPolicy::FailFast ? "FailFast" : "SkipDownstream";
            cfgJson["max_parallel"] = d.config.maxParallel;
            cfgJson["cron_job_id"]  = job.id;
            cfgJson["name"]         = job.name;
            dagBody["config"]       = cfgJson;

            json tasks = json::array();
            for (const auto& spec : d.specs) {
                json jt = core::buildRequestJson(spec.runnerCfg)["task"];
                jt["id"] = spec.id.value;
                json deps = json::array();
                for (const auto& dep : spec.deps) {
                    deps.push_back(dep.value);
                }
                jt["deps"] = std::move(deps);
                tasks.push_back(std::move(jt));
            }
            dagBody["tasks"] = std::move(tasks);

            dagrun::injectRunId(dagBody, runId);
            dagrun::persistRunAndTasks(runId, dagBody, "cron");

            auto r = dag::DagService::instance().runDag(dagBody, runId);
            Logger::info("CronScheduler: Dag job result, id=" + job.id +
                        ", success=" + std::string(r.success ? "true" : "false") +
                        ", message=" + r.message);
            return r.success;
        }

        bool dispatch_template_job(const CronJob& job, const std::string& runId)
        {
            if (!job.templatePayload.has_value()) {
                Logger::error("CronScheduler: templatePayload not set for Template job id=" + job.id);
                return false;
            }
            const auto& payload = *job.templatePayload;
            auto& store = tpl::TemplateStore::instance();
            auto tplOpt = store.get(payload.templateId);
            if (!tplOpt) {
                Logger::error("CronScheduler: template not found, id=" + payload.templateId);
                return false;
            }

            tpl::ParamMap p;
            for (auto& [k, v] : payload.params.items()) {
                p[k] = v;
            }
            auto r = tpl::TemplateRenderer::render(*tplOpt, p);
            if (!r.ok) {
                Logger::error("CronScheduler: template render error: " + r.error);
                return false;
            }

            json rendered = std::move(r.rendered);
            if (!rendered.contains("config") || !rendered["config"].is_object()) {
                rendered["config"] = json::object();
            }
            rendered["config"]["template_id"] = payload.templateId;
            rendered["config"]["cron_job_id"] = job.id;
            if (!rendered.contains("name")) {
                rendered["name"] = tplOpt->name;
            }
            rendered["config"]["name"] = rendered.value("name", tplOpt->name);

            dagrun::injectRunId(rendered, runId);
            dagrun::persistRunAndTasks(runId, rendered, "cron");

            auto dagResult = dag::DagService::instance().runDag(rendered, runId);
            Logger::info("CronScheduler: Template job result, id=" + job.id +
                        ", success=" + std::string(dagResult.success ? "true" : "false") +
                        ", message=" + dagResult.message);
            return dagResult.success;
        }

        bool dispatch_job(const CronJob& job, const std::string& runId)
        {
            switch (job.targetType) {
                case CronTargetType::SingleTask:
                    return dispatch_single_task(job, runId);
                case CronTargetType::Dag:
                    return dispatch_dag_job(job, runId);
                case CronTargetType::Template:
                    return dispatch_template_job(job, runId);
                default:
                    Logger::warn("CronScheduler: Unknown targetType for job id=" + job.id);
                    return false;
            }
        }
    }
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
    
        Logger::info("CronScheduler::addJob, id=" + id + ", name=" + job.name +", spec=" + job.spec);
    
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
                    const std::string runTag = std::to_string(utils::now_millis()) + "_" + utils::random_string(6);
                    const std::string runId  = "cron_" + jobCopy.id + "_" + runTag;

                    // ===== 真正执行逻辑（同步调用） =====
                    try {
                        dispatch_job(jobCopy, runId);
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
