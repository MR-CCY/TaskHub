#include "dag_service.h"
#include "runner/taskRunner.h"
#include "dag/dag_thread_pool.h"
#include "log/logger.h"

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
    core::TaskResult DagService::runDag(const std::vector<dag::DagTaskSpec> &specs, const dag::DagConfig &config, const dag::DagEventCallbacks &callbacks)
    {
        // 构建DAG任务图
        dag::DagBuilder builder;
        for (const auto& spec : specs) {
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

    DagResult DagService::runDag(const json &body)
    {
        std::map<core::TaskId, core::TaskResult> result;
        core::TaskResult tr;
        DagResult dr;
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
        if(body.contains("tasks") && body["tasks"].is_array()){ 
            jTasks = body["tasks"];
        }else if(body.contains("task") && body["task"].is_object()){
            jTasks = json::array();
            jTasks.push_back(body["task"]);
        }else{
            dr.success = false;
            dr.message = "missing or invalid tasks array or object";
            return dr;
        }
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
            dr.success = false;
            dr.message = std::string("parse task failed: ") + ex.what();
            return dr;
        }

        // ===== 4. callbacks =====
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
        // ===== 5. Build DAG =====
        dag::DagBuilder builder;
        for (const auto& spec : specs) {
            builder.addTask(spec);
        }

        auto validateResult = builder.validate();
        if (!validateResult.ok) {
            Logger::error(validateResult.errorMessage);
            dr.success = false;
            dr.message = validateResult.errorMessage;
            return dr;
        }

         // ===== 6. Execute =====
        auto graph = builder.build();
        dag::DagRunContext ctx(cfg, std::move(graph), callbacks);
        dag::DagExecutor executor(runner::TaskRunner::instance());
        core::TaskResult dagResult = executor.execute(ctx);

         // ===== 7. 汇总结果 =====
        const auto& finalMap = ctx.taskResults();
        for (const auto& [id, res] : finalMap) {
            dr.taskResults[id] = res;
        }
        return dr;

    }
}