#include "dag_service.h"
#include "runner/task_runner.h"
#include "dag/dag_thread_pool.h"
#include "dag/dag_run_utils.h"
#include "log/logger.h"
#include "db/dag_run_repo.h"
#include "utils/utils.h"

namespace taskhub::dag {
    DagService::DagService(runner::TaskRunner &runner):
    _executor(runner)
    {
    }
    DagService &DagService::instance()
    {
        static DagService instance(taskhub::runner::TaskRunner::instance());
        return instance;
    }  
    /* 执行DAG任务
    * @param specs DAG中各个任务的规格说明列表
    * @param config DAG运行配置参数
    * @param callbacks DAG事件回调函数集合
    * @return 执行结果，包括状态和消息*/
core::TaskResult DagService::runDag(const std::vector<dag::DagTaskSpec> &specs, const dag::DagConfig &config, const dag::DagEventCallbacks &callbacks, const std::string& runId)
{
    // 构建DAG任务图
    dag::DagBuilder builder;
    for (auto spec : specs) {
        // runId 由外层传入时，写入节点 id 和依赖
        if (!runId.empty()) {
            spec.id.runId = runId;
            for (auto& dep : spec.deps) dep.runId = runId;
        }
        builder.addTask(spec);
    }
        
        // 验证DAG结构是否有效
        auto validateResult = builder.validate();
        if (!validateResult.ok) {
            core::TaskResult r;
            r.status = core::TaskStatus::Failed;
            r.message = validateResult.errorMessage;
            return r;
        }
        
        // 构建最终的DAG图并执行
        auto graph = builder.build();
    dag::DagRunContext ctx(config, std::move(graph), callbacks);
    return _executor.execute(ctx);
}

DagResult DagService::runDag(const json &body, const std::string& runId)
{
    return runDag(body, "manual", runId);
}

DagResult DagService::runDag(const core::TaskConfig& cfg, const std::string& runId)
{
    json body;
    std::string err;
    if (!core::extractDagBody(cfg, body, err)) {
        DagResult dr;
        dr.success = false;
        dr.message = "extractDagBody failed: " + err;
        return dr;
    }
    return runDag(body, "execution", runId);
}

DagResult DagService::runDag(const json &inputBody, const std::string& source, const std::string& inputRunId)
{
    std::map<core::TaskId, core::TaskResult> result;
    DagResult dr;

    // 1. 生成/获取 RunId
    const std::string runId = inputRunId.empty() ? 
        (std::to_string(utils::now_millis()) + "_" + utils::random_string(6)) : inputRunId;

    // 2. 注入 RunId 并持久化
    json body = inputBody;
    // taskhub::dagrun::injectRunId(body, runId);
    taskhub::dagrun::persistRunAndTasks(runId, body, source);

    auto finishRunSafely = [&](int status,int total,int ok,int failed,int skipped,const std::string& message) {
        const long long endTs = utils::now_millis();
        DagRunRepo::instance().finishRun(runId,status,endTs,total,ok,failed,skipped,message);
    };

    // 3. 解析 DagConfig
    dag::DagConfig cfg = dag::DagConfig::from_json(body);
    cfg.dagId = runId;

    // 4. tasks 校验
    json jTasks;
    if(body.contains("tasks") && body["tasks"].is_array()){ 
        jTasks = body["tasks"];
    }else if(body.contains("task") && body["task"].is_object()){
        jTasks = json::array();
        jTasks.push_back(body["task"]);
    }else{
        dr.success = false;
        dr.message = "missing or invalid tasks array or object";
        finishRunSafely(2, 0, 0, 0, 0, dr.message);
        return dr;
    }
    const int totalTasks = static_cast<int>(jTasks.size());

    // 5. 构造 DagTaskSpec
    std::vector<dag::DagTaskSpec> specs;
    try {
        for (const auto& jtask : jTasks) {
            dag::DagTaskSpec spec;
            core::TaskConfig cfgTask = core::parseTaskConfigFromReq(jtask);
            cfgTask.id.runId = runId;
            spec.id        = cfgTask.id;
            spec.runnerCfg = cfgTask;

            if (jtask.contains("deps") && jtask["deps"].is_array()) {
                for (const auto& d : jtask["deps"]) {
                    spec.deps.emplace_back(core::TaskId{ d.get<std::string>(), runId });
                }
            }
            specs.emplace_back(std::move(spec));
        }
    } catch (const std::exception& ex) {
        dr.success = false;
        dr.message = std::string("parse task failed: ") + ex.what();
        finishRunSafely(2, totalTasks, 0, totalTasks, 0, dr.message);
        return dr;
    }

    // 6. callbacks
    std::vector<core::TaskId> nodeStates;
    std::mutex nodeStatesMutex;

    dag::DagEventCallbacks callbacks;
    callbacks.onNodeStatusChanged =
        [&nodeStates, &nodeStatesMutex](const core::TaskId& id, core::TaskStatus st) {
            std::lock_guard<std::mutex> lk(nodeStatesMutex);
            nodeStates.emplace_back(id);
        };

    callbacks.onDagFinished = [](bool success) {
        Logger::info("dag finished, success");
    };

    // 7. Execute
    core::TaskResult dagResult = this->runDag(specs, cfg, callbacks, runId);

    // 8. 汇总结果
    dr.success = (dagResult.status == core::TaskStatus::Success);
    dr.message = dagResult.message;
    // 注意：这里需要从 executor/ctx 获取更详细的任务结果汇总，
    // 由于 runDag 返回的是 TaskResult，这里我们可以通过 DB 或其他方式完善 dr
    // 为了保持逻辑连贯，暂时复用之前的状态统计逻辑，但需要 runDag 协助返回上下文
    // [优化点]：runDag 内部已经处理了大部分逻辑，这里主要是为了兼容 DagResult 返回值
    return dr;
}

} // namespace taskhub::dag
