#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QMap>
#include <QString>

class QGroupBox;

// Manages collapsible panels in the left sidebar.
// Panels are added with addPanel() and can be shown/hidden contextually.
class PanelManager : public QWidget
{
    Q_OBJECT
public:
    explicit PanelManager(QWidget* parent = nullptr);

    // Adds a panel widget inside a collapsible group box.
    // Returns the group box for further configuration.
    QGroupBox* addPanel(const QString& id, const QString& title, QWidget* content);

    // Show/hide panels by id.
    void showPanel(const QString& id);
    void hidePanel(const QString& id);
    void togglePanel(const QString& id);
    bool isPanelVisible(const QString& id) const;

    // Show/hide all panels.
    void showAll();
    void hideAll();

    // Set which panels are visible for a given application mode.
    void applyMode(const QString& mode);

private:
    QScrollArea* scrollArea_;
    QWidget* container_;
    QVBoxLayout* containerLayout_;
    QMap<QString, QGroupBox*> panels_;
};
