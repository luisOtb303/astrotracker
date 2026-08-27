#pragma once

#include <QWidget>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>

// Panel for post-analysis transformation settings.
class TransformPanel : public QWidget
{
    Q_OBJECT
public:
    explicit TransformPanel(QWidget* parent = nullptr);

    int borderMode() const;
    double smoothing() const;
    bool previewEnabled() const;

signals:
    void borderModeChanged(int index);
    void smoothingChanged(double value);
    void previewToggled(bool enabled);

private:
    QComboBox* borderCombo_;
    QDoubleSpinBox* smoothSpin_;
    QCheckBox* previewCheck_;
};
