#pragma once
#include <QWidget>
#include <QJsonArray>

class QStackedWidget;
class CanvasScene;
class CanvasView;
class UndoStack;
class RectItem;
class LineItem;
class DagInspectorWidget;
class NodeInspectorWidget;
class LineInspectorWidget;

// 右侧 Inspector 面板，负责 DAG / 节点 / 连线编辑
class InspectorPanel : public QWidget {
    Q_OBJECT
public:
    InspectorPanel(CanvasScene* scene, UndoStack* undo, CanvasView* view, QWidget* parent = nullptr);
    void setApiClient(class ApiClient* api);
    void setReadOnlyMode(bool ro);

public slots:
    void onSelectionChanged();
    void refreshCurrent();
private slots:
    void saveDagEdits();
    void saveNodeEdits();
    void chooseLineStart();
    void chooseLineEnd();
    void saveLineEdits();
    void runDag();

private:
    void buildUi();
    void showDag();
    void showNode(RectItem* node);
    void showLine(LineItem* line);
    void createDagCron(const int& spec);
    void createNodeCron(const QString& spec);
private:
    CanvasScene* scene_;
    UndoStack* undo_;
    CanvasView* view_;

    QStackedWidget* stack_ = nullptr;
    DagInspectorWidget* dagWidget_ = nullptr;
    NodeInspectorWidget* nodeWidget_ = nullptr;
    LineInspectorWidget* lineWidget_ = nullptr;
    class ApiClient* api_ = nullptr;

    QString dagName_ = "DAG";
    QString dagFailPolicy_ = "SkipDownstream";
    int dagMaxParallel_ = 4;
    RectItem* pendingLineStart_ = nullptr;
    RectItem* pendingLineEnd_ = nullptr;
    LineItem* currentLine_ = nullptr;
    bool pickingStart_ = false;
    bool pickingEnd_ = false;
};
