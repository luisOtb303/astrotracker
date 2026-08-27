#pragma once

#include <QWidget>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QAction>

// Contextual panel for tracking configuration.
// Shows different controls depending on Video or Photo mode.
class TrackingPanel : public QWidget
{
    Q_OBJECT
public:
    explicit TrackingPanel(QWidget* parent = nullptr);

    // Video mode controls
    int trackerIndex() const;
    int borderMode() const;
    double smoothing() const;

    // Photo mode controls
    int methodOverride() const;

    void setMode(bool videoMode);
    void setTrackingActive(bool active);
    void setAnalyzeEnabled(bool enabled);
    void setStatusText(const QString& text);

signals:
    void analyzeRequested();
    void stopRequested();
    void trackerChanged(int index);
    void borderModeChanged(int index);
    void smoothingChanged(double value);

private:
    bool videoMode_ = true;

    // Video controls
    QWidget* videoControls_;
    QComboBox* trackerCombo_;
    QComboBox* borderCombo_;
    QDoubleSpinBox* smoothSpin_;
    QPushButton* analyzeBtn_;
    QPushButton* stopBtn_;

    // Photo controls
    QWidget* photoControls_;
    QComboBox* methodCombo_;
    QLabel* statusLabel_;
    QPushButton* clearOverrideBtn_;
};
