#include "dag_executor.h"
#include <future>
#include <unordered_set>
#include "core/logger.h"
namespace taskhub::dag {
    DagExecutor::DagExecutor(runner::TaskRunner &runner):
    _runner(runner)
    {

    }

    core::TaskResult DagExecutor::execute(DagRunContext &ctx)
    {
        core::TaskResult finalResult;
        finalResult.status = core::TaskStatus::Success;
    
        // TODO Step 1：初始化 readyQueue_（将 indegree=0 的节点入队）
        initReadyQueue(ctx);
    
        // 你可以使用 std::vector<std::future<void>> 来等待所有异步任务
        std::vector<std::future<void>> futures;

        // TODO Step 2：
        //   循环从 readyQueue_ 中取节点，直到：
        //     - readyQueue_ 为空 且 runningCount()==0
        //     - 或者 FailFast 策略下，ctx.isFailed()==true
        //
        //   伪代码：
        //     while (true) {
        //         1) 从 readyQueue_ 里取一个 TaskId（注意加锁）
        //         2) 如果没有可执行节点且 runningCount()==0 → break
        //         3) submitNode(ctx, id)
        //     }
        //
        //   注意：submitNode 内部会调用线程池/异步执行，并在完成时调用 onNodeFinished
        while (true) {
            if (ctx.config().failPolicy == FailPolicy::FailFast && ctx.isFailed()) {
                break;
            }
    
            core::TaskId id;
            {
                std::lock_guard<std::mutex> lk(_readyMutex);
                if (!_readyQueue.empty()) {
                    id.value = _readyQueue.front().value;
                    _readyQueue.pop();
                }
            }
    
            if (id.value.empty()) {
                // 没有新的 ready 节点了
                if (ctx.runningCount() == 0) {
                    break; // 所有任务执行完
                }
    
                // TODO：这里可以选择 sleep 一小会儿，或者用条件变量优化
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }
    
            // 提交执行
            auto fut = submitNode(ctx, id);
            if (fut.valid()) {
                futures.push_back(std::move(fut));
            }
        }
         // TODO Step 3：等待所有异步任务结束（如果你在 submitNode 中用 async 返回 future，可以在此 join）
        for (auto &f : futures) {
            if (f.valid()) {
                f.get();
            }
        }

        // TODO Step 4：根据 ctx.isFailed() 设置 finalResult.status
        if (ctx.isFailed()) {
            finalResult.status = core::TaskStatus::Failed;
        }

        // 通知 DAG 完成
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
    void DagExecutor::onNodeFinished(DagRunContext &ctx, const core::TaskId &id, const core::TaskResult &result)
    {
        auto node = ctx.graph().getNode(id);
        if (!node) {
            ctx.decrementRunning();
            return;
        }

        if (result.ok()) {
            ctx.setNodeStatus(id, core::TaskStatus::Success);

            // ✅ 先处理下游，把新的 ready 节点全部入队
            for (const auto& childId : node->downstream()) {
                auto child = ctx.graph().getNode(childId);
                if (!child) continue;
                int newDeg = child->decrementIndegree();
                if (newDeg == 0) {
                    std::lock_guard<std::mutex> lk(_readyMutex);
                    _readyQueue.push(childId);
                }
            }
        } else {
            ctx.setNodeStatus(id, core::TaskStatus::Failed);

            if (ctx.config().failPolicy == FailPolicy::FailFast) {
                // FailFast：标记失败，主循环会尽快退出，不再消费队列
                ctx.markFailed();
                ctx.decrementRunning();   // ✅ 记得仍然要减一次 runningCount
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

        // ✅ 所有“可能往队列里塞新任务”的逻辑都结束之后，再减 runningCount
        ctx.decrementRunning();
    
    }
}
