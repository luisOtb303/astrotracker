#pragma once

#include "common/CircleF.h"
#include "motion/TrackStatus.h"
#include "stills/PhotoSequenceReader.h"
#include "tracking/DiscTracker.h"

#include <QWidget>
#include <vector>

class QAction;
class QComboBox;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QSlider;
class VideoView;
class PhotoTrackWorker;

// Pestaña "Fotos": abre una secuencia de fotos (JPG/PNG/TIFF/BMP y RAW CR2/CR3)
// y permite sembrar un círculo (posición supuesta del disco), seguir la
// secuencia foto a foto y ver el resultado centrado en el visor derecho.
class PhotoPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PhotoPanel(QWidget* parent = nullptr);
    ~PhotoPanel() override;

    bool isOpen() const { return reader_.isOpen(); }
    int64_t currentIndex() const { return current_; }

    void openImagesDialog();
    void openFolderDialog();
    // Abre directamente una carpeta (desde el menú "Recientes"), sin diálogo.
    void openRecentFolder(const QString& dir);
    QStringList recentFolders() const { return recentFolders_; }

signals:
    // Mensaje en la barra de estado de la ventana principal (timeoutMs 0 =
    // permanente hasta el siguiente mensaje).
    void statusMessage(const QString& msg, int timeoutMs = 0);
    // Progreso de una operación larga; total <= 0 oculta el indicador.
    void workProgress(int done, int total);

private slots:
    void onItemActivated(QListWidgetItem* item);
    void onSliderChanged(int value);
    void showPrev();
    void showNext();
    void showCurrent();
    void onRoiSelected(const QRect& rect);
    void onCircleSelected(const QPointF& center, double radius);
    void setDrawModeCircle(bool circle);
    void runTracking();
    void stopTracking();
    void onWorkerProgress(int done, int total);
    void onWorkerFinished(bool ok, const QString& error, const QVector<double>& results);

private:
    void openPaths(const QStringList& paths);
    void openFolder(const QString& dir);
    void rememberFolder(const QString& dir);
    QString dialogStartDir() const;
    void reloadSequence();
    void buildFilmstrip();
    void updateNavUi();
    void updateTrackingUi();
    void applyCircle(const cv::Point2f& center, float radius);
    void applyViewModes();
    void setTrackingBusy(bool busy);
    void clearSession();

    cv::Mat centeredFrame(const cv::Mat& frame, const DiscTrack& track);
    cv::Mat centeredFrame(const cv::Mat& frame, const CircleF& circle);
    static QPixmap toPixmap(const cv::Mat& bgr);

    PhotoSequenceReader reader_;
    QListWidget* filmstrip_ = nullptr;
    VideoView* view_ = nullptr;
    VideoView* resultView_ = nullptr;
    QSlider* slider_ = nullptr;
    QLabel* indexLabel_ = nullptr;
    QLabel* infoLabel_ = nullptr;
    QAction* prevAction_ = nullptr;
    QAction* nextAction_ = nullptr;
    QAction* analyzeAction_ = nullptr;
    QAction* exportAction_ = nullptr;
    QAction* circleModeAction_ = nullptr;
    QAction* stopAction_ = nullptr;
    QComboBox* borderCombo_ = nullptr;

    bool drawCircleMode_ = true;
    bool trackingBusy_ = false;
    CircleF seedCircle_;
    bool hasSeedCircle_ = false;
    int64_t seedIndex_ = 0;
    std::vector<DiscTrack> tracks_;
    bool analyzed_ = false;
    PhotoTrackWorker* worker_ = nullptr;

    int64_t current_ = 0;
    int displayMaxDim_ = 1600;
    int thumbMaxDim_ = 240;
    QStringList recentFolders_;
    QString lastDir_;
};