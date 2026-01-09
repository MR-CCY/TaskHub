# 嵌套 DAG 卡死 & 结果被吞问题记录

## 现象
- 外层 DAG 的某个节点执行内层 DAG（`execType = Dag`），跑到一半卡住，`onNodeFinished` 不触发，后续节点不再推进。
- 日志里能看到内层 DAG 的 ready 队列被填充，但没有 `dag_node_start` / `dag_node_end` 事件。
- 最终 DAG 结果总是 Success，即使内层节点失败。

## 根因
- 外层 DAG 的节点是在 `DagThreadPool` 的 worker 上执行的，而内层 DAG 也复用同一个池。默认只有 4 个 worker。
- 当外层 DAG 的并行度填满这个池时，内层 DAG 的任务再投递就拿不到空闲线程，形成自锁：外层 worker 阻塞等内层完成，内层却没有 worker 去执行。
- `DagExecutor::execute` 末尾一直返回 Success（失败状态没有透传），导致嵌套失败也被视作成功。

## 改动

### 1. 线程池动态扩容与状态追踪
**文件**: `server/src/dag/dag_thread_pool.{h,cpp}`

#### 新增功能
- **线程状态追踪**:
  - `_activeWorkers`: 追踪存活的线程总数
  - `_busyWorkers`: 追踪正在执行任务的线程数
  - `_isDagWorker` (thread_local): 标识当前线程是否为 DAG worker

- **动态扩容机制** (`maybeSpawnWorker`):
  - 当检测到任务队列有待处理任务且空闲线程不足时，自动创建新 worker
  - 设置硬上限为初始线程数的 4 倍，防止无限扩容
  - 在 `post()` 时自动触发扩容检查

- **资源清理**:
  - `stop()` 时重置所有计数器（`_activeWorkers`, `_busyWorkers`, `_maxWorkers`），避免旧值污染下一次启动

#### 关键修复
- **修复扩容条件逻辑**: 原逻辑为 `spare >= minSpare && active < _maxWorkers`（AND），导致即使有足够空闲线程仍会扩容。修正为 `spare >= minSpare || total >= _maxWorkers`（OR），只在空闲不足且未达上限时扩容。
- **添加硬上限**: `_maxWorkers` 初始化为 `num_workers * 4`，防止高并发嵌套场景下线程数无限增长。

### 2. 嵌套 DAG 同步执行
**文件**: `server/src/dag/dag_executor.cpp`

#### 核心改动
- **同步执行策略**: 在 `submitNodeAsync()` 中，检测到当前线程是 DAG worker 时，直接同步执行节点任务，不再投递到线程池队列。
  ```cpp
  if (dag::DagThreadPool::instance().isDagWorkerThread()) {
      // 同步执行节点
      result = _runner.run(cfg, nullptr);
      onNodeFinished(ctx, id, result);
      return;
  }
  ```
- **彻底规避死锁**: 避免了"外层 worker 等待内层任务，内层任务排队等待 worker"的循环依赖。

#### 失败状态透传
- 在 `execute()` 末尾检查 `ctx.isFailed()`，若失败则设置 `finalResult.status = Failed`
- 在 `onNodeFinished()` 中，遇到失败类状态（Failed/Timeout/Canceled）立即调用 `ctx.markFailed()`
- 移除了 FailFast 分支中的重复 `ctx.markFailed()` 调用

### 3. 策略层简化
**文件**: `server/src/execution/dag_strategy.cpp`

- **移除冗余扩容**: 删除了嵌套 DAG 场景下的主动 `maybeSpawnWorker(1)` 调用，因为节点会同步执行，不需要额外 worker。

## 设计权衡

### 同步执行 vs 并行度
**权衡**: 在 DAG worker 中同步执行嵌套 DAG 节点会降低并行度。

**分析**:
- **优点**: 彻底避免死锁，确保 `onNodeFinished` 总是被调用
- **缺点**: 如果内层 DAG 执行时间长，会长时间占用外层 DAG 的 worker，外层其他节点无法并行执行

**结论**: 在当前场景下，**避免死锁优先级更高**。系统可靠性优先于性能优化。

### 动态扩容 vs 固定线程池
**权衡**: 动态扩容增加了复杂度，但提供了更好的灵活性。

**分析**:
- **优点**: 缓解线程饥饿，提高嵌套 DAG 的并发能力
- **缺点**: 线程数可能膨胀，增加上下文切换开销

**结论**: 通过硬上限（4 倍初始值）平衡灵活性和资源控制。

## 建议
- **监控线程池状态**: 关注线程池扩容日志，如果频繁扩容到上限，考虑增加初始线程数或优化 DAG 并行度配置。
- **避免过深嵌套**: 嵌套层级过深会导致同步执行链过长，影响整体并行度。建议嵌套层级不超过 3 层。
- **排查工具**: 如需进一步排查，可用 thread dump 确认 DAG worker 堆栈是否卡在 `DagExecutionStrategy::execute -> DagService::runDag -> DagExecutor::execute`。

## 测试验证
- ✅ 嵌套 DAG 不再卡死，`onNodeFinished` 总是被调用
- ✅ 内层 DAG 失败状态正确传播到外层
- ✅ 线程池扩容机制正常工作，不会无限增长
- ✅ 资源清理正确，重启线程池不会有旧状态污染
