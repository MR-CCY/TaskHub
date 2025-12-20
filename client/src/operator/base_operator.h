// base_operator.h
#pragma once
#include <QList>
#include "view/canvasview.h"

class BaseOperator {
public:
    explicit BaseOperator(CanvasView* view, BaseOperator* parent = nullptr);
    virtual ~BaseOperator();

    bool doOperator(bool canUndo = true);

    void addChild(BaseOperator* op);

protected:
    // 子类实现具体的业务
    virtual bool innerDo(bool canUndo) = 0;

private:
    CanvasView* view_;
    BaseOperator* parent_;
    QList<BaseOperator*> children_;
};