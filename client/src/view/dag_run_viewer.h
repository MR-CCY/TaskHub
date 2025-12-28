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
};
