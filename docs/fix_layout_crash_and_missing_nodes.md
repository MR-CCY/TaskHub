# 修复布局崩溃与节点丢失问题

## 问题描述

在导入包含 HttpCall 节点（H_1）的 DAG JSON 时，发现以下问题：

1. **导入崩溃**：应用程序在导入 JSON 文件后触发自动布局时崩溃
2. **节点丢失**：导入成功后，H_1（HttpCall）和 S_2（Shell）节点不可见
3. **连线异常**：连线渐变色显示正常，但节点本身缺失

## 根本原因分析

### 1. 崩溃问题

**症状**：
```
LayoutTask::execute() 在调用 view()->scene()->itemsBoundingRect() 时崩溃
```

**原因**：
- `LayoutTask` 构造函数缺少 `CanvasView*` 参数
- `Task` 基类的 `view_` 成员未被初始化
- 调用 `view()` 返回 `nullptr`，导致空指针访问

**调用链**：
```
ImportTask::execute() 
  └─> new LayoutTask(scene_, mgr)  // ❌ 缺少 view 参数
      └─> view()->scene()->itemsBoundingRect()  // 💥 崩溃
```

### 2. 节点丢失问题

**症状**：
```
[Layout] Node: "H_1" pos: QPointF(520,0)      // ✅ 布局算法设置正确
[Layout] Node: "H_1" scenePos: QPointF(0,0)   // ❌ 但场景位置错误
```

**原因**：
`RectItem::itemChange()` 中的碰撞检测逻辑阻止了节点移动：

```cpp
if (change == ItemPositionChange) {
    QPointF newPos = value.toPointF();
    
    if (scene()) {
        // 检测节点是否会与容器内部区域相交
        if (newBounds.intersects(inner)) {
            return pos();  // ❌ 返回旧位置，阻止移动！
        }
    }
    return newPos;
}
```

**问题流程**：
1. 布局算法调用 `node->setPos(520, 0)` 设置 H_1 位置
2. Qt 触发 `ItemPositionChange` 事件
3. 碰撞检测发现新位置可能与 DAG_1 容器相交
4. 返回旧位置 `pos()` = `(0, 0)`
5. H_1 被阻止移动，停留在 `(0, 0)`
6. H_1 与 S_2 重叠，被遮挡

## 解决方案

### 修复 1：补充 LayoutTask 的 CanvasView 参数

**修改文件**：
- `layout_task.h`
- `layout_task.cpp`  
- `import_task.cpp`
- `canvasbench.cpp`

**关键改动**：

```cpp
// layout_task.h
class LayoutTask : public Task {
public:
    explicit LayoutTask(CanvasScene* scene, CanvasView* view, QObject* parent = nullptr);
    // ...
};

// layout_task.cpp
LayoutTask::LayoutTask(CanvasScene* scene, CanvasView* view, QObject* parent)
    : Task(10, parent), scene_(scene) {
    setView(view);  // ✅ 设置 view，避免空指针
}

// import_task.cpp (调用点 1)
auto* layoutTask = new LayoutTask(scene_, view_, mgr);

// canvasbench.cpp (调用点 2)
auto* task = new LayoutTask(scene_, view_, this);
```

### 修复 2：布局期间禁用碰撞检测

**修改文件**：
- `rect_item.h`
- `rect_item.cpp`
- `layout_task.cpp`

**设计思路**：
添加全局标志控制碰撞检测行为，布局时临时禁用，完成后恢复。

**关键改动**：

```cpp
// rect_item.h - 添加静态标志
class RectItem : public BaseItem {
public:
    // 布局期间禁用碰撞检测
    static void setCollisionDetectionEnabled(bool enabled);
    static bool isCollisionDetectionEnabled();
private:
    static inline bool collisionDetectionEnabled_ = true;
};

// rect_item.cpp - 检查标志
if (scene() && isCollisionDetectionEnabled()) {
    // 碰撞检测逻辑
}

// layout_task.cpp - 布局时控制
void layoutNodes(...) {
    RectItem::setCollisionDetectionEnabled(false);  // 禁用
    
    // 执行布局算法
    for (auto* node : list) {
        node->setPos(colX, rowSlots * spacingY);  // ✅ 可以自由移动
    }
    
    RectItem::setCollisionDetectionEnabled(true);  // 恢复
}
```

## 验证结果

### 崩溃问题
✅ **已修复** - 导入 JSON 不再崩溃

### 节点丢失问题
✅ **已修复** - 所有节点正确显示在预期位置

**修复前日志**：
```
[Layout] Node: "S_2" scenePos: QPointF(0,0)    // ❌ 停留在原点
[Layout] Node: "H_1" scenePos: QPointF(0,0)    // ❌ 与 S_2 重叠
```

**修复后日志**：
```
[Layout] Node: "S_2" scenePos: QPointF(260,0)  // ✅ 正确位置
[Layout] Node: "H_1" scenePos: QPointF(520,0)  // ✅ 正确位置
```

## 适用场景

该修复适用于以下场景：
1. ✅ 通过 Import 功能导入 JSON 文件
2. ✅ DAG 运行监控窗口加载服务端 JSON
3. ✅ 所有涉及自动布局的场景

## 影响范围

### 正面影响
- 解决了所有类型节点在自动布局时的丢失问题
- 修复了容器节点边界计算导致的节点遮挡
- 保持了用户手动拖动时的碰撞检测功能

### 潜在风险
- **无** - 碰撞检测仅在布局期间禁用，用户交互时正常工作

## 相关文件清单

| 文件路径 | 修改类型 | 说明 |
|---------|---------|------|
| `tasks/layout_task.h` | 接口变更 | 添加 CanvasView 参数 |
| `tasks/layout_task.cpp` | 实现修改 | 设置 view, 控制碰撞检测 |
| `tasks/import_task.cpp` | 调用修正 | 传入 view_ 参数 |
| `view/canvasbench.cpp` | 调用修正 | 传入 view_ 参数 |
| `Item/rect_item.h` | 新增 API | 碰撞检测开关 |
| `Item/rect_item.cpp` | 逻辑优化 | 检查碰撞开关标志 |

## 技术细节

### Qt Graphics Item Position Change 机制

```cpp
QVariant itemChange(GraphicsItemChange change, const QVariant &value) {
    if (change == ItemPositionChange) {
        QPointF newPos = value.toPointF();
        // 可以修改并返回新位置
        return modifiedPos;  // 返回值会成为实际位置
    }
}
```

**关键点**：
- `ItemPositionChange` 在位置改变**之前**触发
- 返回值决定最终位置
- 返回 `pos()` 会阻止移动

### 布局算法坐标系统

```
节点局部坐标 (pos)           场景坐标 (scenePos)
     ↓                            ↓
(260, 0) ──────────────→  (260, 0)  ← 无父节点时相同
     
(260, 0) ──────────────→  (260 + parentX, 0 + parentY)  ← 有父节点时需累加
```

## 经验教训

1. **Qt 事件机制的副作用**：需要注意 `itemChange()` 可能在非预期时机阻止操作
2. **调试技巧**：对比 `pos()` 和 `scenePos()` 可快速定位坐标问题
3. **设计原则**：碰撞检测应区分用户交互和程序化操作

## 后续优化建议

1. 考虑将碰撞检测改为 RAII 模式：
   ```cpp
   {
       CollisionGuard guard(false);  // 自动禁用
       // 布局代码
   }  // 自动恢复
   ```

2. 添加单元测试验证布局算法的正确性
3. 考虑使用 `QGraphicsItem::ItemIgnoresTransformations` 等标志优化性能
