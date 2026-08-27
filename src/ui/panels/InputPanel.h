#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>

// Panel for opening files and displaying material information.
class InputPanel : public QWidget
{
    Q_OBJECT
public:
    explicit InputPanel(QWidget* parent = nullptr);

    void setMaterialInfo(const QString& info);

signals:
    void openVideo();
    void openPhotos();
    void openProject();

private:
    QPushButton* openVideoBtn_;
    QPushButton* openPhotosBtn_;
    QPushButton* openProjectBtn_;
    QLabel* materialInfoLabel_;
};
