#pragma once

#include <QMainWindow>
#include <memory>

class VideoView;
class IVideoReader;
class QLabel;
class QSlider;
class QTimer;
class QAction;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void openFile();
    void openPath(const QString& path);
    void playPause();
    void stop();
    void stepForward();
    void stepBackward();
    void onTimer();
    void onRoiSelected(const QRect& rect);

private:
    void setupUi();
    void showCurrentFrame();
    void updateTransportUi();
    QString formatTime(int64_t us) const;

    VideoView* view_ = nullptr;
    QLabel* frameLabel_ = nullptr;
    QLabel* timeLabel_ = nullptr;
    QSlider* slider_ = nullptr;
    QTimer* timer_ = nullptr;
    QAction* playAction_ = nullptr;

    std::unique_ptr<IVideoReader> reader_;
    int64_t currentUs_ = 0;
    int64_t stepUs_ = 40000;
    int64_t totalUs_ = 0;
    int64_t totalFrames_ = 0;
    QRect roi_;
};