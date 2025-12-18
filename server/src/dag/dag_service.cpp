#include "dag_service.h"
#include "runner/taskRunner.h"
#include "dag/dag_thread_pool.h"
#include "core/logger.h"
namespace taskhub::api {
    DagService::DagService(runner::TaskRunner &runner):
    _executor(runner)
    {
        dag::DagThreadPool::instance().start(4);
    }
    DagService &DagService::instance()
    {
        static DagService instance(taskhub::runner::TaskRunner::instance());
        return instance;
        // TODO: 在此处插入 return 语句
    }
    core::TaskResult DagService::runDag(const std::vector<dag::DagTaskSpec> &specs, const dag::DagConfig &config, const dag::DagEventCallbacks &callbacks)
    {
        // TODO Step 1：构建 builder，添加所有任务
        dag::DagBuilder builder;
        for (const auto& spec : specs) {
            builder.addTask(spec);
        }

        // TODO Step 2：validate
        auto validateResult = builder.validate();
        if (!validateResult.ok) {
            core::TaskResult r;
            r.status = core::TaskStatus::Failed;
            r.message = validateResult.errorMessage;
            return r;
        }

        // TODO Step 3：构建图 + 运行
        auto graph = builder.build();
        
        dag::DagRunContext ctx(config, std::move(graph), callbacks);
        return _executor.execute(ctx);
    }
    std::map<core::TaskId, core::TaskResult> DagService::runDag(const json &body)
    {
        std::map<core::TaskId, core::TaskResult> result;
        core::TaskResult tr;

        // ===== 1. 解析 DagConfig =====
        dag::DagConfig cfg;
        if (body.contains("config") && body["config"].is_object()) {
            const auto& jcfg = body["config"];
            std::string policy = jcfg.value("fail_policy", "SkipDownstream");
            if (policy == "FailFast") {
                cfg.failPolicy = dag::FailPolicy::FailFast;
            } else {
                cfg.failPolicy = dag::FailPolicy::SkipDownstream;
            }
            cfg.maxParallel = jcfg.value("max_parallel", 4u);
        }

               // ===== 2. tasks 校验 =====
        //创建json数组
        json jTasks;
        Logger::debug(body.dump());
        if(body.contains("tasks") && body["tasks"].is_array()){ 
            jTasks = body["tasks"];
        }else if(body.contains("task") && body["task"].is_object()){
            jTasks = json::array();
            jTasks.push_back(body["task"]);
        }else{
            tr.status = core::TaskStatus::Failed;
            tr.message = "missing or invalid tasks array or object";
            result[core::TaskId{"_system"}] = tr;
            return result;
        }
        Logger::debug(jTasks.dump());
        // ===== 3. 构造 DagTaskSpec =====
        std::vector<dag::DagTaskSpec> specs;
        try {
            for (const auto& jtask : jTasks) {
                Logger::debug(jtask.dump());
                dag::DagTaskSpec spec;

                core::TaskConfig cfgTask = core::parseTaskConfigFromReq(jtask);
                spec.id        = cfgTask.id;
                spec.runnerCfg = cfgTask;

                if (jtask.contains("deps") && jtask["deps"].is_array()) {
                    for (const auto& d : jtask["deps"]) {
                        spec.deps.emplace_back(core::TaskId{ d.get<std::string>() });
                    }
                }

                specs.emplace_back(std::move(spec));
            }
        } catch (const std::exception& ex) {
            tr.status = core::TaskStatus::Failed;
            tr.message = std::string("parse task failed: ") + ex.what();
            result[core::TaskId{"_system"}] = tr;
            return result;
        }

        // ===== 4. callbacks =====
        std::vector<json> nodeStates;
        std::mutex nodeStatesMutex;

        dag::DagEventCallbacks callbacks;
        callbacks.onNodeStatusChanged =
            [&nodeStates, &nodeStatesMutex](const core::TaskId& id, core::TaskStatus st) {
                std::lock_guard<std::mutex> lk(nodeStatesMutex);
                nodeStates.push_back({
                    {"id", id.value},
                    {"status", static_cast<int>(st)}
                });
            };

        callbacks.onDagFinished = [](bool success) {
            Logger::info("dag finished, success");
        };
        // ===== 5. Build DAG =====
        dag::DagBuilder builder;
        for (const auto& spec : specs) {
            builder.addTask(spec);
        }

        auto validateResult = builder.validate();
        if (!validateResult.ok) {
            Logger::error(validateResult.errorMessage);
            tr.status = core::TaskStatus::Failed;
            tr.message = validateResult.errorMessage;
            return result;
        }

         // ===== 6. Execute =====
        auto graph = builder.build();
        dag::DagRunContext ctx(cfg, std::move(graph), callbacks);
        dag::DagExecutor executor(runner::TaskRunner::instance());
        core::TaskResult dagResult = executor.execute(ctx);

         // ===== 7. 汇总结果 =====
        const auto& finalMap = ctx.taskResults();
        
        return finalMap;

    }
}