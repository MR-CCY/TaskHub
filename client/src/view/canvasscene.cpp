// canvas_scene.cpp
#include "canvasscene.h"

CanvasScene::CanvasScene(QObject* parent) : QGraphicsScene(parent) {
    // 设置场景背景颜色，方便调试
    setBackgroundBrush(Qt::lightGray);
    setSceneRect(-5000, -5000, 10000, 10000); // 设置一个足够大的初始区域，避免点击边缘节点时发生跳变
}

CanvasScene::~CanvasScene() = default;