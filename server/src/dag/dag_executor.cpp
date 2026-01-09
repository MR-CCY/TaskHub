#include "dag_executor.h"
#include <future>
#include <unordered_set>
#include "log/logger.h"
#include "dag_thread_pool.h"
#include "ws/ws_log_streamer.h"
#include "db/task_run_repo.h"
#include "utils/utils.h"
namespace taskhub::dag {
    DagExecutor::DagExecutor(runner::TaskRunner &runner):
    _runner(runner)
    {

    }

    core::TaskResult DagExecutor::execute(DagRunContext &ctx)
    {
        core::TaskResult finalResult;
        finalResult.status = core::TaskStatus::Success;
    
        // 1. 读取并限制 maxParallel
        _maxParallel = static_cast<int>(ctx.config().maxParallel);
        if (_maxParallel <= 0) {
            _maxParallel = 1;
        }
    
        // 2. 初始化 ready 队列（入度为 0 的节点入队）
        initReadyQueue(ctx);
    
        // 3. 调度循环：直到没有 ready 节点 且 没有运行中的任务
        while (true) {
            // 3.1 尽量填满并行度：从 readyQueue 里取节点，丢给线程池执行
            while (ctx.runningCount() < _maxParallel) {
                core::TaskId id;

                {
                    std::lock_guard<std::mutex> lk(_readyMutex);
                    if (!_readyQueue.empty()) {
                        id = _readyQueue.front();
                        _readyQueue.pop();
                    }
                }

                if (id.value.empty()) {
                    // 当前没有更多 ready 节点可以提交
                    break;
                }

                // 如果该节点已经被标记为不可运行（例如被上游失败 SkipDownstream 标成 Skipped），
                // 即使还留在 readyQueue 里，也必须丢弃，避免“本该跳过却被执行”的竞态。
                if (auto n = ctx.graph().getNode(id)) {
                    auto st = n->status();
                    if (st == core::TaskStatus::Skipped ||
                        st == core::TaskStatus::Failed ||
                        st == core::TaskStatus::Timeout ||
                        st == core::TaskStatus::Canceled ||
                        st == core::TaskStatus::Success ||
                        st == core::TaskStatus::Running) {
                        continue;
                    }
                }

                // FailFast 模式下，如果已经失败，就不再提交新任务（但会等待已有任务跑完）
                if (ctx.config().failPolicy == FailPolicy::FailFast && ctx.isFailed()) {
                    // 直接丢弃这个 id，不提交
                    continue;
                }

                // 使用线程池异步执行节点
                submitNodeAsync(ctx, id);
            }

            // 3.2 检查结束条件：没有 ready 节点，且没有运行中的任务
            {
                std::lock_guard<std::mutex> lk(_readyMutex);
                if (_readyQueue.empty() && ctx.runningCount() == 0) {
                    break; // 所有节点执行完毕
                }
            }

            // 3.3 使用条件变量等待“状态改变”：有新 ready / 有任务完成 / FailFast 失败
            std::unique_lock<std::mutex> lk(_cvMutex);
            _cv.wait(lk, [this, &ctx,&finalResult] {
                // 有新 ready 节点？
                {
                    std::lock_guard<std::mutex> qlk(_readyMutex);
                    if (!_readyQueue.empty()) {
                        // finalResult=ctx.taskResults()[ctx.id];
                        return true;
                    }
                }
                // 所有任务都执行完了？
                if (ctx.runningCount() == 0) {
                    return true;
                }
                // FailFast：已经失败？
                if (ctx.config().failPolicy == FailPolicy::FailFast && ctx.isFailed()) {
                    return true;
                }
                return false;
            });
        }
    
        // 4. 根据 ctx.isFailed() 设置最终结果，避免 DAG 失败被吞掉
        if (ctx.isFailed()) {
            finalResult.status = core::TaskStatus::Failed;
            finalResult.message = "dag failed";
        }
    
        // 5. 通知 DAG 完成
        ctx.finish(finalResult.ok());
    
        return finalResult;
    }
    void DagExecutor::initReadyQueue(DagRunContext &ctx)
    {
        for (const auto& [idStr, node] : ctx.graph().nodes()) {
            Logger::info("initReadyQueue node id=" + node->id().value +
                         ", indegree=" + std::to_string(node->indegree()));
            if (node->indegree() == 0) {
                std::lock_guard<std::mutex> lk(_readyMutex);
                _readyQueue.push(node->id());
                Logger::info("  -> push to readyQueue: " + node->id().value);
                // WS event: node becomes ready
                ws::WsLogStreamer::instance().pushTaskEvent(
                    node->id().value,
                    "dag_node_ready",
                    {
                        {"indegree", node->indegree()},
                        {"run_id", node->id().runId}
                    }
                );
            }
        }
    }
    /**
     * 提交一个DAG节点以异步执行
     * 
     * 该方法会获取指定ID的节点，将其状态设置为运行中，
     * 并启动一个异步任务来执行节点任务，最后调用完成回调。
     * 
     * @param ctx DAG运行上下文，包含图结构和执行状态信息
     * @param id 要执行的节点的任务ID
     * @return std::future<void> 可用于等待节点执行完成的对象
     */
    void DagExecutor::submitNodeAsync(DagRunContext &ctx, const core::TaskId &id)
    {
        auto* graph = &ctx.graph();
        auto node = graph->getNode(id);
        if (!node) return;

        // 二次防线：节点可能在进入 readyQueue 后，被上游失败标记为 Skipped。
        // 这里必须再次检查，避免竞态下仍然被提交执行。
        auto cur = node->status();
        if (cur == core::TaskStatus::Skipped ||
            cur == core::TaskStatus::Failed ||
            cur == core::TaskStatus::Timeout ||
            cur == core::TaskStatus::Canceled ||
            cur == core::TaskStatus::Success ||
            cur == core::TaskStatus::Running) {
            return;
        }

        ctx.setNodeStatus(id, core::TaskStatus::Running);
        ctx.incrementRunning();
        if (!id.runId.empty()) {
            TaskRunRepo::instance().markRunning(id.runId, id.value, utils::now_millis());
        }
        // WS event: node execution starts
        ws::WsLogStreamer::instance().pushTaskEvent(
            id.value,
            "dag_node_start",
            {
                {"exec_type", TaskExecTypetoString(node->runnerConfig().execType)},
                {"queue", node->runnerConfig().queue},
                {"run_id", id.runId}
            }
        );

        const auto priority = node->runnerConfig().priority;

        // 在 DAG worker 线程中嵌套执行 DAG 时，可直接同步执行节点，避免再次排队导致的竞态
        if (dag::DagThreadPool::instance().isDagWorkerThread()) {
            auto nodeInner = ctx.graph().getNode(id);
            core::TaskResult result;
            if (nodeInner) {
                const auto& cfg = nodeInner->runnerConfig();
                result = _runner.run(cfg, nullptr);
            } else {
                result.status  = core::TaskStatus::Failed;
                result.message = "Node not found in graph";
            }
            onNodeFinished(ctx, id, result);
            return;
        }

        // 如果是嵌套 DAG 节点，主动确保有足够的 worker
        // 因为嵌套 DAG 会同步执行，长时间占用 worker
        if (node->runnerConfig().execType == core::TaskExecType::Dag ||
            node->runnerConfig().execType == core::TaskExecType::Template) {
            // 主动扩容，确保有空闲 worker 可用
            dag::DagThreadPool::instance().maybeSpawnWorker(1);
        }

        dag::DagThreadPool::instance().post([this, &ctx, id]() {
            auto nodeInner = ctx.graph().getNode(id);
            core::TaskResult result;

            if (nodeInner) {
                const auto& cfg = nodeInner->runnerConfig();
                result = _runner.run(cfg, nullptr);
            } else {
                result.status  = core::TaskStatus::Failed;
                result.message = "Node not found in graph";
            }
    
            onNodeFinished(ctx, id, result);
        }, priority);
    }
    void DagExecutor::onNodeFinished(DagRunContext &ctx, const core::TaskId &id, const core::TaskResult &result)
    {
        auto node = ctx.graph().getNode(id);
        if (!node) {
            ctx.decrementRunning();
            _cv.notify_one();
            return;
        }
        ctx.setTaskResults(id, result);
        if (!id.runId.empty()) {
            TaskRunRepo::instance().markFinished(id.runId, id.value, result, utils::now_millis());
        }
        auto st = result.status;
        // WS event: node finished (first-class status)
        ws::WsLogStreamer::instance().pushTaskEvent(
            id.value,
            "dag_node_end",
            {
                {"status", core::TaskStatusTypetoString(st)},
                {"duration_ms", result.durationMs},
                {"exit_code", result.exitCode},
                {"run_id", id.runId}
            }
        );
        if (result.ok()) {
            ctx.setNodeStatus(id, core::TaskStatus::Success);
            // ✅ 先处理下游，把新的 ready 节点全部入队
            for (const auto& childId : node->downstream()) {
                auto child = ctx.graph().getNode(childId);
                if (!child) continue;
                auto childSt = child->status();
                if (childSt == core::TaskStatus::Skipped ||
                    childSt == core::TaskStatus::Failed ||
                    childSt == core::TaskStatus::Timeout ||
                    childSt == core::TaskStatus::Canceled) {
                    continue;
                }

                int newDeg = child->decrementIndegree();
                if (newDeg == 0) {
                    {
                        std::lock_guard<std::mutex> lk(_readyMutex);
                        _readyQueue.push(childId);
                    }
                    // WS event: child becomes ready
                        ws::WsLogStreamer::instance().pushTaskEvent(
                            childId.value,
                            "dag_node_ready",
                            {
                                {"indegree", 0},
                                {"parent", id.value},
                                {"run_id", childId.runId}
                            }
                        );
                }
            }
        } else {
              // 1）先把原始状态写进去（Failed / Timeout / Canceled）
            switch (st) {
            case core::TaskStatus::Timeout:
                ctx.setNodeStatus(id, core::TaskStatus::Timeout);
                break;
            case core::TaskStatus::Canceled:
                ctx.setNodeStatus(id, core::TaskStatus::Canceled);
                break;
            default:
                ctx.setNodeStatus(id, core::TaskStatus::Failed);
                break;
            }
            // 2）在“策略”上统一当成“失败类”
            const bool isFailureLike =(st == core::TaskStatus::Failed|| st == core::TaskStatus::Timeout || st == core::TaskStatus::Canceled);
            if (isFailureLike) {
                // 记录 DAG 已进入失败态，供最终结果和外层嵌套 DAG 感知
                ctx.markFailed();
                if (ctx.config().failPolicy == FailPolicy::FailFast) {
                    // FailFast：标记失败，主循环会尽快退出，不再消费队列
                    ctx.decrementRunning();   // ✅ 记得仍然要减一次 runningCount
                    _cv.notify_one();    // 必须唤醒一次，否则可能卡在 wait 上
                    return;
                }

                if (ctx.config().failPolicy == FailPolicy::SkipDownstream) {
                    // 这里沿用你原来的 BFS，把所有下游标记为 Skipped
                    std::queue<core::TaskId> toVisit;
                    std::unordered_set<std::string> visited;

                    for (const auto& childId : node->downstream()) {
                        if (visited.insert(childId.value).second) {
                            toVisit.push(childId);
                        }
                    }

                    while (!toVisit.empty()) {
                        auto childId = toVisit.front();
                        toVisit.pop();

                        auto child = ctx.graph().getNode(childId);
                        if (!child) {
                            continue;
                        }

                        ctx.setNodeStatus(childId, core::TaskStatus::Skipped);
                        ctx.setTaskResults(childId, core::TaskResult{
                            .status = core::TaskStatus::Skipped
                        });
                        if (!childId.runId.empty()) {
                            TaskRunRepo::instance().markSkipped(
                                childId.runId,
                                childId.value,
                                "skip_downstream upstream=" + id.value,
                                utils::now_millis());
                        }
                        // WS event: node skipped due to upstream failure
                        ws::WsLogStreamer::instance().pushTaskEvent(
                            childId.value,
                            "dag_node_skipped",
                            {
                                {"reason", "skip_downstream"},
                                {"upstream", id.value},
                                {"run_id", childId.runId}
                            }
                        );
                        for (const auto& grandChildId : child->downstream()) {
                            if (visited.insert(grandChildId.value).second) {
                                toVisit.push(grandChildId);
                            }
                        }
                    }
                }
            }
        }
        // ✅ 所有“可能往队列里塞新任务”的逻辑都结束之后，再减 runningCount
        ctx.decrementRunning();
        _cv.notify_one();
    }
}
