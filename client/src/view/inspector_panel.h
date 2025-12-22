#pragma once
#include <QWidget>

class QStackedWidget;
class QLineEdit;
class QCheckBox;
class QComboBox;
class QSpinBox;
class QPushButton;
class QLabel;
class CanvasScene;
class CanvasView;
class UndoStack;
class RectItem;
class LineItem;

// 右侧 Inspector 面板，负责 DAG / 节点 / 连线编辑
class InspectorPanel : public QWidget {
    Q_OBJECT
public:
    InspectorPanel(CanvasScene* scene, UndoStack* undo, CanvasView* view, QWidget* parent = nullptr);

public slots:
    void onSelectionChanged();

private slots:
    void saveDagEdits();
    void saveNodeEdits();
    void chooseLineStart();
    void chooseLineEnd();
    void saveLineEdits();

private:
    void buildUi();
    void showDag();
    void showNode(RectItem* node);
    void showLine(LineItem* line);

private:
    CanvasScene* scene_;
    UndoStack* undo_;
    CanvasView* view_;

    QStackedWidget* stack_ = nullptr;
    QWidget* dagPage_ = nullptr;
    QWidget* nodePage_ = nullptr;
    QWidget* linePage_ = nullptr;

    // DAG
    QLineEdit* dagNameEdit_ = nullptr;
    QComboBox* dagFailPolicyCombo_ = nullptr;
    QSpinBox* dagMaxParallelSpin_ = nullptr;
    QPushButton* dagSaveBtn_ = nullptr;
    QString dagName_ = "DAG";
    QString dagFailPolicy_ = "SkipDownstream";
    int dagMaxParallel_ = 4;

    // Node
    QLineEdit* nodeNameEdit_ = nullptr;
    QLineEdit* nodeCmdEdit_ = nullptr;
    QLineEdit* nodeTimeoutEdit_ = nullptr;
    QLineEdit* nodeRetryEdit_ = nullptr;
    QLineEdit* nodeQueueEdit_ = nullptr;
    QCheckBox* nodeCaptureBox_ = nullptr;
    QLineEdit* httpMethodEdit_ = nullptr;
    QLineEdit* httpBodyEdit_ = nullptr;
    QLineEdit* shellCwdEdit_ = nullptr;
    QLineEdit* shellShellEdit_ = nullptr;
    QLineEdit* innerTypeEdit_ = nullptr;
    QLineEdit* innerCmdEdit_ = nullptr;
    QPushButton* nodeSaveBtn_ = nullptr;

    // Line
    QLabel* lineStartLabel_ = nullptr;
    QLabel* lineEndLabel_ = nullptr;
    QPushButton* lineChooseStartBtn_ = nullptr;
    QPushButton* lineChooseEndBtn_ = nullptr;
    QPushButton* lineSaveBtn_ = nullptr;
    RectItem* pendingLineStart_ = nullptr;
    RectItem* pendingLineEnd_ = nullptr;
    LineItem* currentLine_ = nullptr;
    bool pickingStart_ = false;
    bool pickingEnd_ = false;
};
