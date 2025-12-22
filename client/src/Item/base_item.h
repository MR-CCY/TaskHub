#pragma once
#include <QGraphicsObject>
#include <QMap>
#include <QVariant>

class QGraphicsScene;
class GraphModel;     // 你现有的 model（可先不接）
class UndoStack;      // 你的 UndoStack（继承 QUndoStack）

class BaseItem : public QGraphicsObject {
    Q_OBJECT
public:
    enum class Kind { Node, Edge };

    explicit BaseItem(Kind k, QGraphicsItem* parent = nullptr);
    ~BaseItem() override = default;

    Kind kind() const { return kind_; }

    // ---- “统一属性容器” ----
    const QMap<QString, QVariant>& properties() const { return props_; }
    QVariant prop(const QString& key) const { return props_.value(key); }
    void setProp(const QString& key, const QVariant& v) { props_[key] = v; }

    // 工厂 URI（序列化会用到）
    QString factoryUri() const { return factoryUri_; }
    void setFactoryUri(const QString& uri) { factoryUri_ = uri; }

    // Scene / Model / Undo 注入（Bench 装配时 set）
    void attachContext(QGraphicsScene* s, GraphModel* m, UndoStack* u);

    QGraphicsScene* sceneCtx() const { return scene_; }
    GraphModel* model() const { return model_; }
    UndoStack* undo() const { return undo_; }

    // ---- Q1-1：命令工厂接口位（严格链路：Operator -> Item::exec -> Command -> Undo）----
    // 这里只定义接口位，不在本阶段实现具体 Command（后面你要我再给 Create/Move/Edge Command）
    virtual void execCreateCmd(bool canUndo = true) = 0;
    virtual void execMoveCmd(const QPointF& newPos, bool canUndo = true) = 0;

protected:
    Kind kind_;
    QMap<QString, QVariant> props_;
    QString factoryUri_;

    QGraphicsScene* scene_ = nullptr;
    GraphModel* model_ = nullptr;
    UndoStack* undo_ = nullptr;
};
