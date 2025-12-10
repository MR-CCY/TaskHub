#include "dag_executor.h"
#include <future>
#include <unordered_set>
#include "core/logger.h"
#include "dag_thread_pool.h"
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
            _cv.wait(lk, [this, &ctx] {
                // 有新 ready 节点？
                {
                    std::lock_guard<std::mutex> qlk(_readyMutex);
                    if (!_readyQueue.empty()) {
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
    
        // 4. 根据 ctx.isFailed() 设置最终结果
        if (ctx.isFailed()) {
            finalResult.status = core::TaskStatus::Failed;
        }
    
        // 5. 通知 DAG 完成
        ctx.finish(finalResult.ok());
    
        return finalResult;
    }
    void DagExecutor::initReadyQueue(DagRunContext &ctx)
    {
        // TODO：
        //   遍历 ctx.graph().nodes()
        //   把 indegree()==0 且 初始状态为 Pending 的节点加入 readyQueue_
        //
        //   for (auto& [idStr, node] : ctx.graph().nodes()) {
        //       if (node->indegree() == 0) {
        //           std::lock_guard<std::mutex> lk(readyMutex_);
        //           readyQueue_.push(node->id());
        //       }
        //   }

        for (const auto& [idStr, node] : ctx.graph().nodes()) {
            Logger::info("initReadyQueue node id=" + node->id().value +
                         ", indegree=" + std::to_string(node->indegree()));
            if (node->indegree() == 0) {
                std::lock_guard<std::mutex> lk(_readyMutex);
                _readyQueue.push(node->id());
                Logger::info("  -> push to readyQueue: " + node->id().value);
            }
        }
    }
    std::future<void> DagExecutor::submitNode(DagRunContext &ctx, const core::TaskId &id)
    {
        auto* graph = &ctx.graph();
        auto node = graph->getNode(id);
        if (!node) return {};
    
        // TODO Step 1：更新状态为 Running
        ctx.setNodeStatus(id, core::TaskStatus::Running);
        ctx.incrementRunning();
    
        // TODO Step 2：把任务提交到线程池或 async
        //
        //   示例（你可以改成使用自己的线程池）：
        //
        //   std::async(std::launch::async, [this, &ctx, id]() {
        //       auto node = ctx.graph().getNode(id);
        //       auto cfg  = node->runnerConfig();
        //       core::TaskResult result = runner_.run(cfg, /*cancelFlag*/ nullptr);
        //       onNodeFinished(ctx, id, result);
        //   });
        //
        //   注意：onNodeFinished 里会更新 indegree、状态、ready 队列等
    
        return std::async(std::launch::async, [this, &ctx, id]() {
            auto nodeInner = ctx.graph().getNode(id);
            core::TaskResult result;
    
            if (nodeInner) {
                // 这里调用你自己的 TaskRunner 实现
                result = _runner.run(nodeInner->runnerConfig(), nullptr);
            } else {
                result.status = core::TaskStatus::Failed;
                result.message = "DagExecutor: node not found in graph";
            }
    
            onNodeFinished(ctx, id, result);
        });
    }
    void DagExecutor::submitNodeAsync(DagRunContext &ctx, const core::TaskId &id)
    {
        auto* graph = &ctx.graph();
        auto node = graph->getNode(id);
        if (!node) return;
    
        ctx.setNodeStatus(id, core::TaskStatus::Running);
        ctx.incrementRunning();
    
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
        });
    }
    void DagExecutor::onNodeFinished(DagRunContext &ctx, const core::TaskId &id, const core::TaskResult &result)
    {
        auto node = ctx.graph().getNode(id);
        if (!node) {
            ctx.decrementRunning();
            _cv.notify_one();
            return;
        }

        auto st = result.status;
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
                    std::lock_guard<std::mutex> lk(_readyMutex);
                    _readyQueue.push(childId);
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
                if (ctx.config().failPolicy == FailPolicy::FailFast) {
                    // FailFast：标记失败，主循环会尽快退出，不再消费队列
                    ctx.markFailed();
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
