// canvas_scene.cpp
#include "canvasscene.h"

CanvasScene::CanvasScene(QObject* parent) : QGraphicsScene(parent) {
    // 设置场景背景颜色，方便调试
    setBackgroundBrush(Qt::lightGray);
    setSceneRect(0, 0, 800, 600); // 默认大小
}

CanvasScene::~CanvasScene() = default;