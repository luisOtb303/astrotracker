#include "ui/panels/PanelManager.h"

#include <QGroupBox>
#include <QScrollArea>
#include <QVBoxLayout>

PanelManager::PanelManager(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    scrollArea_ = new QScrollArea(this);
    scrollArea_->setWidgetResizable(true);
    scrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea_->setFrameShape(QFrame::NoFrame);

    container_ = new QWidget();
    containerLayout_ = new QVBoxLayout(container_);
    containerLayout_->setContentsMargins(4, 4, 4, 4);
    containerLayout_->setSpacing(4);
    containerLayout_->addStretch(1);

    scrollArea_->setWidget(container_);
    mainLayout->addWidget(scrollArea_);
}

QGroupBox* PanelManager::addPanel(const QString& id, const QString& title, QWidget* content)
{
    auto* group = new QGroupBox(title, container_);
    auto* lay = new QVBoxLayout(group);
    lay->setContentsMargins(8, 16, 8, 8);
    lay->setSpacing(4);
    lay->addWidget(content);

    // Insert before the stretch at the end.
    containerLayout_->insertWidget(containerLayout_->count() - 1, group);
    panels_[id] = group;
    return group;
}

void PanelManager::showPanel(const QString& id)
{
    if (auto* p = panels_.value(id))
        p->setVisible(true);
}

void PanelManager::hidePanel(const QString& id)
{
    if (auto* p = panels_.value(id))
        p->setVisible(false);
}

void PanelManager::togglePanel(const QString& id)
{
    if (auto* p = panels_.value(id))
        p->setVisible(!p->isVisible());
}

bool PanelManager::isPanelVisible(const QString& id) const
{
    if (auto* p = panels_.value(id))
        return p->isVisible();
    return false;
}

void PanelManager::showAll()
{
    for (auto* p : panels_)
        p->setVisible(true);
}

void PanelManager::hideAll()
{
    for (auto* p : panels_)
        p->setVisible(false);
}

void PanelManager::applyMode(const QString& mode)
{
    hideAll();
    if (mode == "empty") {
        showPanel("input");
    } else if (mode == "loaded_video") {
        showPanel("input");
        showPanel("object");
        showPanel("tracking");
    } else if (mode == "loaded_photo") {
        showPanel("input");
        showPanel("object");
        showPanel("tracking");
    } else if (mode == "analyzed") {
        showPanel("input");
        showPanel("object");
        showPanel("tracking");
        showPanel("transform");
        showPanel("export");
    } else if (mode == "processing") {
        showPanel("input");
        showPanel("tracking");
    }
}
