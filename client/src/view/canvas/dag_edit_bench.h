#pragma once
#include "canvasbench.h"
#include "view/inspector/inspector_panel.h"

class QToolBar;
class QAction;
class QSplitter;
class InspectorPanel;
class NodePaletteWidget;

// CanvasBench with toolbar/actions and inspector for DAG editing.
class DagEditBench : public CanvasBench {
  Q_OBJECT
public:
  explicit DagEditBench(QWidget *parent = nullptr);
  ~DagEditBench() override = default;
  void setApiClient(class ApiClient *api);

private:
  void buildUi();
  void wireUi();

private:
  QToolBar *toolbar_ = nullptr;
  QAction *actUndo_ = nullptr;
  QAction *actRedo_ = nullptr;
  QAction *actConnect_ = nullptr;
  QAction *actDelete_ = nullptr;
  QAction *actExportJson_ = nullptr;
  QAction *actImportJson_ = nullptr;
  QAction *actLayout_ = nullptr;
  InspectorPanel *inspector_ = nullptr;
  NodePaletteWidget *nodePanel_ = nullptr;
  class ApiClient *api_ = nullptr;
};
