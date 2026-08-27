#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>

// Panel for export configuration and action.
class ExportPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ExportPanel(QWidget* parent = nullptr);

    void setSelectionInfo(int selected, int total);

signals:
    void exportRequested();

private:
    QPushButton* exportBtn_;
    QLabel* selectionLabel_;
};
