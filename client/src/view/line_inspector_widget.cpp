#include "line_inspector_widget.h"

#include <QFormLayout>
#include <QLabel>
#include <QPushButton>

LineInspectorWidget::LineInspectorWidget(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void LineInspectorWidget::buildUi()
{
    auto* form = new QFormLayout(this);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(6);

    startLabel_ = new QLabel("-", this);
    endLabel_ = new QLabel("-", this);
    chooseStartBtn_ = new QPushButton(tr("选择起点"), this);
    chooseEndBtn_ = new QPushButton(tr("选择终点"), this);
    saveBtn_ = new QPushButton(tr("保存连线"), this);

    form->addRow(tr("起点"), startLabel_);
    form->addRow(chooseStartBtn_);
    form->addRow(tr("终点"), endLabel_);
    form->addRow(chooseEndBtn_);
    form->addRow(saveBtn_);

    connect(chooseStartBtn_, &QPushButton::clicked, this, &LineInspectorWidget::chooseStartRequested);
    connect(chooseEndBtn_, &QPushButton::clicked, this, &LineInspectorWidget::chooseEndRequested);
    connect(saveBtn_, &QPushButton::clicked, this, &LineInspectorWidget::saveRequested);
}

void LineInspectorWidget::setEndpoints(const QString& startName, const QString& endName)
{
    startLabel_->setText(startName);
    endLabel_->setText(endName);
}
