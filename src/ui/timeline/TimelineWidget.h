#pragma once

#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QHBoxLayout>

// Timeline widget with slider, frame counter, and time display.
class TimelineWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TimelineWidget(QWidget* parent = nullptr);

    void setRange(int min, int max);
    void setValue(int value);
    void setEnabled(bool enabled);
    void setFrameLabel(int current, int total);
    void setTimeLabel(const QString& time);
    int value() const;

signals:
    void valueChanged(int value);

private:
    QSlider* slider_;
    QLabel* frameLabel_;
    QLabel* timeLabel_;
};
