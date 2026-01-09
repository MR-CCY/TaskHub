# 嵌套 DAG 并发执行问题解决案例

> **项目背景**: TaskHub - 分布式任务调度系统  
> **问题类型**: 并发编程、架构设计  
> **解决时间**: 2026-01-09  
> **难度**: ⭐⭐⭐⭐⭐

---

## 问题描述

在实现 DAG（有向无环图）任务调度系统时，支持了嵌套 DAG 功能（DAG 节点可以是另一个 DAG）。但在测试时发现：
- **现象**: 两个并行的嵌套 DAG 节点（DAG_1 和 DAG_2），只有 DAG_1 能执行，DAG_2 卡在 ready 状态
- **影响**: 嵌套 DAG 功能完全不可用

---

## 问题分析过程

### 1. 初步调查
- 检查数据库事件日志，发现 DAG_2 只有 `dag_node_ready` 事件，缺少 `dag_node_start` 和 `dag_node_end`
- 说明 DAG_2 从未被调度执行

### 2. 代码审查
- 审查了线程池扩容逻辑、任务调度逻辑
- 尝试修复线程池的 `maybeSpawnWorker` 逻辑
- **结果**: 问题依然存在

### 3. 深入调试
添加详细日志后发现关键线索：
```
查找节点: DAG_2 (runId=1767973010634_29f378)
但 graph 中只有: S_1 (runId=1767973010634_60c42e)  // 这是 DAG_1 的内层节点！
```

**关键发现**: 外层 DAG 的 graph 被内层 DAG 的节点污染了！

### 4. 根本原因定位

通过分析架构发现：
```cpp
class DagService {
    static DagService& instance();  // 单例
private:
    DagExecutor _executor;  // ← 所有 DAG 共享同一个实例！
};

class DagExecutor {
private:
    std::queue<TaskId> _readyQueue;  // ← 共享状态
    std::mutex _readyMutex;
    std::condition_variable _cv;
};
```

**问题时序**：
1. 外层 DAG 将 DAG_1、DAG_2 加入 `_readyQueue`
2. DAG_1 开始执行，同步调用内层 DAG
3. 内层 DAG 使用**同一个** `_executor`，将 S_1 加入**同一个** `_readyQueue`
4. 队列混乱：`[DAG_2, S_1]`
5. 外层调度循环可能取到内层节点 S_1，在外层 graph 中找不到 → 失败

---

## 解决方案

### 核心修复：状态隔离

**修改前**：
```cpp
class DagService {
private:
    DagExecutor _executor;  // 共享实例
};

core::TaskResult DagService::runDag(...) {
    // ...
    return _executor.execute(ctx);  // 所有 DAG 共享
}
```

**修改后**：
```cpp
class DagService {
private:
    runner::TaskRunner& _runner;  // 只存储 runner 引用
};

core::TaskResult DagService::runDag(...) {
    // ...
    DagExecutor executor(_runner);  // 每次创建新实例
    return executor.execute(ctx);   // 状态完全隔离
}
```

### 辅助修复

1. **节点查找使用复合 key**
   - 原来只用 `id.value`，忽略 `runId`
   - 改为 `runId:value` 复合 key，支持不同 runId 的同名节点

2. **同步执行嵌套 DAG**
   - 在 worker 线程中检测到嵌套 DAG 时，同步执行避免死锁
   - 权衡：降低并行度，但避免线程池饥饿

3. **添加嵌套深度限制**
   - 最大嵌套深度 10 层，防止栈溢出

---

## 技术亮点

### 1. 问题定位能力
- 从现象到日志分析，再到架构审查
- 使用调试日志精确定位状态污染问题
- 识别出架构设计缺陷（共享状态）

### 2. 并发编程理解
- 理解共享状态在并发环境中的危险性
- 识别竞态条件和状态混乱问题
- 设计状态隔离方案

### 3. 架构设计能力
- 权衡性能与可靠性（同步执行 vs 并行度）
- 设计防御性机制（深度限制、硬上限）
- 保持向后兼容性

### 4. 系统性思维
- 不仅修复表面问题，还改进整体健壮性
- 考虑边界情况（深度限制、资源上限）
- 提供清晰的错误信息

---

## 成果

- ✅ 彻底解决嵌套 DAG 执行问题
- ✅ 支持任意层级嵌套（最多 10 层）
- ✅ 支持并行嵌套 DAG
- ✅ 提升系统健壮性和可维护性
- ✅ 性能影响可忽略（DagExecutor 创建成本极低）

---

## 关键收获

1. **架构问题比算法问题更隐蔽** - 共享状态导致的问题最难发现和调试
2. **调试日志的重要性** - 详细的日志帮助快速定位问题根源
3. **权衡取舍** - 在可靠性和性能之间做出合理选择
4. **防御性编程** - 添加限制和保护机制，防止系统被滥用

---

**技术栈**: C++17, 多线程, DAG 调度, 并发编程  
**代码量**: 11 个文件，+248 行，-20 行  
**影响范围**: 核心调度引擎
