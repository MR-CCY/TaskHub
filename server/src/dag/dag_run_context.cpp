#include "dag_run_context.h"
namespace taskhub::dag {
    DagRunContext::DagRunContext(const DagConfig &cfg, DagGraph graph, DagEventCallbacks callbacks)
    : _config(cfg)
    , _graph(std::move(graph))
    , _callbacks(std::move(callbacks))
    {
    }
    void DagRunContext::incrementRunning()
    {
        _running.fetch_add(1, std::memory_order_relaxed);
    }
    void DagRunContext::decrementRunning()
    {
        _running.fetch_sub(1, std::memory_order_relaxed);
    }
    int DagRunContext::runningCount() const
    {
        return _running.load(std::memory_order_relaxed);
    }
    void DagRunContext::markFailed()
    {
        _failed.store(true, std::memory_order_release);
    }
    bool DagRunContext::isFailed() const
    {
        return _failed.load(std::memory_order_acquire);
    }
    void DagRunContext::setNodeStatus(const core::TaskId &id, core::TaskStatus st)
    {
        //Step 1：更新节点自身状态
        std::function<void(const core::TaskId&, core::TaskStatus)> cb;
        {
            std::lock_guard<std::mutex> lk(_mutex);
            auto node = _graph.getNode(id);
            if (node) {
                node->setStatus(st);
                // _finalStatus[id] = st;  // 总是覆盖成“最后一次”状态
            }
            cb = _callbacks.onNodeStatusChanged;
        }
        //Step 2：调用回调（注意要判空）
        if (cb) {
            cb(id, st);
        }
    }
    void DagRunContext::setTaskResults(const core::TaskId &id, core::TaskResult st)
    {
        std::lock_guard<std::mutex> lk(_mutex);
        _taskResults[id] = st;
    }
    std::map<core::TaskId, core::TaskResult> DagRunContext::taskResults()
    {
        std::lock_guard<std::mutex> lk(_mutex);
        return _taskResults;
    }
    void DagRunContext::finish(bool success)
    {
        // DAG 完成时调用，可做：
        //   - 打日志
        //   - 调用 onDagFinished 回调
        auto cb = _callbacks.onDagFinished;
        if (cb) {
            cb(success);
        }
    }
    // std::map<core::TaskId, core::TaskStatus> DagRunContext::finalStatus()
    // {
    //     std::lock_guard<std::mutex> lk(_mutex);
    //     return _finalStatus;
    // }
}
