#pragma once
#include "dag_builder.h"
#include "dag_executor.h"
//todo：需要和handnle里面的抽成一个
namespace taskhub::api {

class DagService {
public:
    DagService(runner::TaskRunner& runner);
    static DagService& instance();

    // 高层接口：传入一组 DagTaskSpec，执行整个 DAG
    core::TaskResult runDag(const std::vector<dag::DagTaskSpec>& specs,
                            const dag::DagConfig& config,
                            const dag::DagEventCallbacks& callbacks);

private:
    dag::DagExecutor _executor;
};

} // namespace taskhub::api