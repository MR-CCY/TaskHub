// canvas_scene.h
#pragma once
#include <QGraphicsScene>

class CanvasView;

class CanvasScene : public QGraphicsScene {
    Q_OBJECT
public:
    explicit CanvasScene(QObject* parent = nullptr);
    ~CanvasScene() override;
    
    // 这里可以加一些简单的图层管理，目前先留空
};