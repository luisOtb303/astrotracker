#pragma once

#include "export/PipelineWorker.h"
#include "processing/Pipeline.h"

#include <QMainWindow>
#include <QVector>
#include <memory>
#include <vector>

class VideoView;
class IVideoReader;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QProgressBar;
class QSlider;
class QTimer;
class QAction;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Abre un vídeo desde la línea de comandos.
    void openPath(const QString& path);

private slots:
    void openFile();
    void playPause();
    void stop();
    void stepForward();
    void stepBackward();
    void onTimer();
    void onRoiSelected(const QRect& rect);

    void startAnalyze();
    void togglePreview(bool enabled);
    void startExport();
    void onWorkerProgress(int done, int total);
    void onAnalyzeFinished(bool ok, const QString& error, QVector<QPointF> offsets,
                           int frames, int valid, double meanConfidence);
    void onExportFinished(bool ok, const QString& error, int frames, int valid);
    void onWorkerFinished();

private:
    void setupUi();
    void showCurrentFrame();
    void updateTransportUi();
    void updateStabilizationUi();
    QString formatTime(int64_t us) const;
    PipelineSettings currentSettings() const;
    cv::Mat displayFrame(const cv::Mat& src, int64_t frameIndex) const;
    void launchWorker(const PipelineWorker::Request& req);
    void setBusy(bool busy);

    VideoView* view_ = nullptr;
    VideoView* resultView_ = nullptr;
    QLabel* frameLabel_ = nullptr;
    QLabel* timeLabel_ = nullptr;
    QSlider* slider_ = nullptr;
    QTimer* timer_ = nullptr;
    QAction* playAction_ = nullptr;
    QAction* analyzeAction_ = nullptr;
    QAction* previewAction_ = nullptr;
    QAction* exportAction_ = nullptr;
    QComboBox* trackerCombo_ = nullptr;
    QComboBox* borderCombo_ = nullptr;
    QDoubleSpinBox* smoothSpin_ = nullptr;
    QProgressBar* progressBar_ = nullptr;

    std::unique_ptr<IVideoReader> reader_;
    QString inPath_;
    int64_t currentUs_ = 0;
    int64_t stepUs_ = 40000;
    int64_t totalUs_ = 0;
    int64_t totalFrames_ = 0;
    QRect roi_;

    PipelineWorker* worker_ = nullptr;
    std::vector<cv::Point2f> offsets_;
    bool previewEnabled_ = false;
    int64_t startIndex_ = 0;
    int64_t startUs_ = 0;
};