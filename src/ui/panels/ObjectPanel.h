#pragma once

#include <QWidget>
#include <QComboBox>
#include <QLabel>

// Panel for selecting the tracked object profile.
class ObjectPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ObjectPanel(QWidget* parent = nullptr);

    void setProfile(int index);
    int profile() const;

signals:
    void profileChanged(int index);

private:
    QComboBox* profileCombo_;
    QLabel* descriptionLabel_;
};
