#pragma once

#include "canvasbench.h"
#include <QJsonObject>

// 仅用于展示（只读导入）的 Bench
class DagRunViewerBench : public CanvasBench {
    Q_OBJECT
public:
    explicit DagRunViewerBench(QWidget* parent = nullptr);

    // 从 DAG JSON（包含 tasks/config）导入到画布
    bool loadDagJson(const QJsonObject& obj);
    void setNodeStatus(const QString& nodeId, const QColor& color);
    void setNodeStatusLabel(const QString& nodeId, const QString& label);
    void selectNode(const QString& nodeId);

    // 获取所有 Remote 类型的节点 ID（用于监控轮询）
    QStringList getRemoteNodes() const;

    // 只读访问底层指针供监控面板使用
    CanvasScene* canvasScene() const { return scene(); }
    UndoStack* benchUndo() const { return undoStack(); }
    CanvasView* benchView() const { return view(); }

signals:
    void nodeSelected(const QString& nodeId);

private:
    QHash<QString, RectItem*> idMap_;
};
