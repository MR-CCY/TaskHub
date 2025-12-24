#pragma once
#include <QJsonObject>
#include <QString>

class CanvasScene;

// Build DAG JSON (same shape as导出/运行接口)，returns empty object if no nodes.
QJsonObject buildDagJson(CanvasScene* scene, const QString& failPolicy, int maxParallel, const QString& dagName = QString());
