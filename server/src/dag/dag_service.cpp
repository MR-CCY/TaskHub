#include "dag_service.h"
#include "runner/taskRunner.h"
#include "dag/dag_thread_pool.h"
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
}