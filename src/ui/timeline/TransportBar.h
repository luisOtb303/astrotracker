#pragma once

#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>

// Transport control bar for video playback.
class TransportBar : public QWidget
{
    Q_OBJECT
public:
    explicit TransportBar(QWidget* parent = nullptr);

    void setPlaying(bool playing);
    void setEnabled(bool enabled);

signals:
    void stepBackward();
    void playPause();
    void stepForward();
    void stop();

private:
    QPushButton* playBtn_;
    QPushButton* stopBtn_;
};
