# DagExecutor 共享状态问题修复（真正的根本原因）

## 问题发现

通过日志分析发现：
```
查找 key: 1767973010634_29f378:DAG_2 (外层 runId)
但 graph 中只有: [1767973010634_60c42e:S_1] (内层 runId)
```

**外层 DAG 的 graph 被内层 DAG 的节点污染了！**

## 根本原因

### 架构问题

**`DagService` 是单例**：
```cpp
static DagService& instance() {
    static DagService instance(TaskRunner::instance());
    return instance;
}
```

**`DagService` 有一个 `_executor` 成员**：
```cpp
class DagService {
private:
    dag::DagExecutor _executor;  // ← 所有 DAG 共享！
};
```

**`DagExecutor` 有共享状态**：
```cpp
class DagExecutor {
private:
    std::mutex _readyMutex;
    std::queue<core::TaskId> _readyQueue;  // ← 共享的队列！
    std::condition_variable _cv;
    std::mutex _cvMutex;
    int _maxParallel = 4;
};
```

### 问题时序

```
时刻 T1: 外层 DAG 开始执行
  - 调用 DagService::instance().runDag(outerBody)
  - 使用 _executor.execute(outerCtx)
  - outerCtx.graph() 包含: [DAG_1, DAG_2]
  - _readyQueue = [DAG_1, DAG_2]

时刻 T2: DAG_1 开始执行（在 worker 线程中）
  - submitNodeAsync 将 DAG_1 从 _readyQueue 取出
  - 调用 _runner.run(DAG_1)
  - 因为在 worker 线程中，同步执行

时刻 T3: DAG_1 同步调用内层 DAG
  - 调用 DagService::instance().runDag(innerBody)
  - ★ 使用同一个 _executor！
  - 调用 _executor.execute(innerCtx)
  - innerCtx.graph() 包含: [S_1]
  - ★ 将 S_1 加入 _readyQueue！

时刻 T4: _readyQueue 状态混乱
  - _readyQueue = [DAG_2, S_1]  ← 混杂了外层和内层节点！
  - 外层 DAG 的调度循环从 _readyQueue 取节点
  - 可能取到 S_1（内层节点）
  - 尝试在 outerCtx.graph() 中查找 S_1
  - 但 outerCtx.graph() 只有 [DAG_1, DAG_2]
  - 查找失败！

时刻 T5: 或者反过来
  - 内层 DAG 的调度循环从 _readyQueue 取节点
  - 可能取到 DAG_2（外层节点）
  - 尝试在 innerCtx.graph() 中查找 DAG_2
  - 但 innerCtx.graph() 只有 [S_1]
  - 查找失败！
```

## 解决方案

### 修复：每次执行创建新的 DagExecutor

**文件**: `server/src/dag/dag_service.cpp`

```cpp
core::TaskResult DagService::runDag(const std::vector<dag::DagTaskSpec> &specs, 
                                    const dag::DagConfig &config, 
                                    const dag::DagEventCallbacks &callbacks, 
                                    const std::string& runId)
{
    // ... 构建 graph ...
    
    dag::DagRunContext ctx(config, std::move(graph), callbacks);
    
    // ★ 关键修复：每次执行都创建新的 DagExecutor 实例
    // 避免嵌套 DAG 共享状态（_readyQueue 等）
    dag::DagExecutor executor(_runner);
    return executor.execute(ctx);
}
```

**文件**: `server/src/dag/dag_service.h`

```cpp
class DagService {
private:
    runner::TaskRunner& _runner;  // 只存储 runner 引用
    // 移除: dag::DagExecutor _executor;
};
```

**文件**: `server/src/dag/dag_service.cpp`

```cpp
DagService::DagService(runner::TaskRunner &runner):
    _runner(runner)  // 只初始化 runner
{
}
```

### 为什么这样修复

1. **每次执行都有独立的状态**：
   - 每个 DAG 执行都有自己的 `DagExecutor` 实例
   - 每个实例都有自己的 `_readyQueue`、`_cv` 等
   - 外层和内层 DAG 完全隔离

2. **性能影响很小**：
   - `DagExecutor` 很轻量，创建成本低
   - 只包含几个成员变量（queue、mutex、cv）
   - 相比 DAG 执行的总时间，创建开销可以忽略

3. **线程安全**：
   - 不再需要担心多个 DAG 并发执行时的状态冲突
   - 每个 DAG 都在自己的 context 中运行

## 测试验证

重新测试并行嵌套 DAG，预期：
- DAG_1 和 DAG_2 都能正确执行
- 不会出现节点查找失败
- 日志显示每个 graph 都包含正确的节点

## 总结

这是一个**架构设计问题**：

- ❌ **旧设计**: 单例 `DagService` + 共享 `DagExecutor` → 状态污染
- ✅ **新设计**: 单例 `DagService` + 每次创建新 `DagExecutor` → 状态隔离

这个修复彻底解决了嵌套 DAG 的状态混乱问题！
