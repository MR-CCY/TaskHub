// DagExecutor.h
#pragma once
#include <queue>
#include <mutex>
#include <future>
#include "dag_run_context.h"
#include "runner/taskRunner.h"

namespace taskhub::dag {

class DagExecutor {
public:
    DagExecutor(runner::TaskRunner& runner);

    // 同步执行（当前线程阻塞直到 DAG 结束）
    core::TaskResult execute(DagRunContext& ctx);

private:
    runner::TaskRunner& _runner;
    std::mutex _readyMutex;
    std::queue<core::TaskId> _readyQueue;
    std::condition_variable _cv;      // ★ 新增：调度线程等待任务完成/ready 变化
    std::mutex _cvMutex;              // ★ 与 _cv 配套
    int _maxParallel = 4;             // 可以从 DagConfig 里读
    
    void initReadyQueue(DagRunContext& ctx);
    void submitNodeAsync(DagRunContext& ctx, const core::TaskId& id);
    void onNodeFinished(DagRunContext& ctx,const core::TaskId& id,const core::TaskResult& result);
};

} // namespace taskhub::dag
