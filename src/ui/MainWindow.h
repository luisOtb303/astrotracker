#pragma once

#include "common/ImageAdjust.h"
#include "export/PipelineWorker.h"
#include "processing/AdjustWorker.h"
#include "processing/HotPixelMap.h"
#include "processing/Pipeline.h"
#include "stills/PhotoProject.h"

#include <QMainWindow>
#include <QStringList>
#include <QVector>
#include <memory>
#include <vector>

class VideoView;
class PhotoPanel;
class IVideoReader;
class VideoExportWorker;
class HotPixelScanWorker;
class QTimer;
class QAction;
class QMenu;
class QDockWidget;
class QPlainTextEdit;
class QCheckBox;
class QTabWidget;
class QProgressBar;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QGroupBox;
class QSlider;
struct Frame;

// New UI components
class ThemeManager;
class InfoPanel;
class ImageAdjustPanel;
class TimelineWidget;
class ShortcutManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void openPath(const QString& path);
    // Abre un archivo externo (doble click, CLI): .atracker → proyecto, resto → vídeo.
    void openExternalFile(const QString& path);

protected:
    void closeEvent(QCloseEvent* event) override;
    // Atrapa Espacio/Return/S/J/L/P cuando el foco está en un control que no los
    // usa (botones, campos de texto), para no lanzar la acción global.
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void openFile();
    void playPause();
    void stop();
    void stepForward();
    void stepBackward();
    void onTimer();
    void onRoiSelected(const QRect& rect);

    void startAnalyze();
    void onFitFrame();
    void stopProcessing();
    void togglePreview(bool enabled);
    void startExportVideo();
    void startExportPhotos();
    void runExportDialog(bool toVideo);
    void onWorkerProgress(int done, int total);
    void onAnalyzeFinished(bool ok, const QString& error, QVector<QPointF> offsets,
                           QVector<TrackSample> samples,
                           int frames, int valid, double meanConfidence);
    void onExportFinished(bool ok, const QString& error, int frames, int valid);
    void onVideoExportFinished(bool ok, const QString& error, int frames);
    void onVideoExportFrame(int64_t index);
    void onWorkerFinished();
    void onLogMessage(int level, const QString& text);

    void openProjectDialog();
    void closeProject();
    void saveProjectTriggered();
    void saveProjectAsTriggered();

    void showAbout();
    void showLicenses();
    void aboutQt();

private:
    void setupUi();
    void installShortcuts();
    QWidget* createVideoPage();
    void openPhotos();
    void populateRecentsMenu(QMenu* menu);
    void rememberVideoPath(const QString& path);
    // Lee el frame SIGUIENTE del vídeo (avance secuencial del decoder).
    void advanceToNextFrame();
    // Repinta el frame ya cargado, sin tocar el decoder. Es lo que necesitan los
    // cambios de ajuste, el modo de borde o el toggle de vista previa: si usaran
    // advanceToNextFrame() el vídeo avanzaría un frame con cada interacción.
    void refreshCurrentFrame();
    void presentFrame(const Frame& frame);
    void requestVideoAdjustPreview();
    void onVideoAdjusted(const cv::Mat& out, quint64 seq);
    void ensureVideoAdjustWorker();
    // Análisis temporal de píxeles calientes. Va en su propio hilo con un
    // lector propio: no toca reader_ y no mueve la posición del visor.
    void onAnalyzeHotPixels();
    void onHotScanFinished(const HotPixelMap::Result& result);
    void stopHotScan();
    void updateTrackCircle(int64_t frameIndex);
    void updateTransportUi();
    void updateStabilizationUi();
    void updateExportMenus();
    void updatePanelMode();
    void updateTimelineForPhotos(int64_t index = -1);
    void restoreDocks();
    QString formatTime(int64_t us) const;
    PipelineSettings currentSettings() const;
    cv::Mat displayFrame(const cv::Mat& src, int64_t frameIndex) const;
    void launchWorker(const PipelineWorker::Request& req);
    void setBusy(bool busy);

    PhotoProject collectProject() const;
    PhotoProjectVideo collectVideo() const;
    void applyVideo(const PhotoProjectVideo& v);
    void closeVideo();
    bool anyBusy() const;
    bool hasContent() const;
    bool saveProject(const QString& path);
    bool saveProjectAs();
    bool openProject(const QString& path);
    bool confirmContinue();
    void writeAutosave();
    void markProjectModified();
    void rememberProjectPath(const QString& path);
    void refreshProjectUi();

    // --- Old widgets (still used internally, will be wrapped by new components) ---
    VideoView* view_ = nullptr;
    VideoView* resultView_ = nullptr;
    PhotoPanel* photosPanel_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    QTimer* timer_ = nullptr;
    QAction* playAction_ = nullptr;
    QToolBar* transportToolBar_ = nullptr;
    QAction* analyzeAction_ = nullptr;
    QAction* previewAction_ = nullptr;
    QAction* exportVideoAction_ = nullptr;
    QAction* exportPhotosAction_ = nullptr;
    QAction* fitAction_ = nullptr;
    QAction* stopAction_ = nullptr;

    // --- New UI components ---
    InfoPanel* infoPanel_ = nullptr;
    TimelineWidget* timeline_ = nullptr;
    ShortcutManager* shortcuts_ = nullptr;
    QDockWidget* infoDock_ = nullptr;

    // Log dock (kept from old design)
    QProgressBar* progressBar_ = nullptr;
    QDockWidget* logDock_ = nullptr;
    QPlainTextEdit* logView_ = nullptr;
    QCheckBox* debugCheck_ = nullptr;
    QAction* logDockAction_ = nullptr;
    QAction* trackDockAction_ = nullptr;
    QAction* infoDockAction_ = nullptr;

    // Project actions
    QAction* openProjectAction_ = nullptr;
    QAction* closeProjectAction_ = nullptr;
    QAction* saveProjectAction_ = nullptr;
    QAction* saveProjectAsAction_ = nullptr;
    QAction* menuExportVideoAction_ = nullptr;
    QAction* menuExportPhotosAction_ = nullptr;
    QString projectPath_;
    bool projectDirty_ = false;
    QStringList recentProjects_;

    // Old tracking dock widgets (kept for backward compatibility during refactor)
    QComboBox* trackerCombo_ = nullptr;
    QComboBox* borderCombo_ = nullptr;
    QDoubleSpinBox* smoothSpin_ = nullptr;
    QComboBox* profileCombo_ = nullptr;
    QDockWidget* trackDock_ = nullptr;
    QComboBox* photoMethodCombo_ = nullptr;
    QLabel* photoStatusLabel_ = nullptr;
    QPushButton* clearOverrideBtn_ = nullptr;
    QGroupBox* videoGroup_ = nullptr;

    // --- Image Adjust dock ---
    ImageAdjustPanel* imgPanel_ = nullptr;
    QDockWidget* imgDock_ = nullptr;
    QAction* imgDockAction_ = nullptr;
    ImageAdjust videoAdjust_;
    // Carpeta de la última exportación de vídeo, para abrirla al terminar.
    QString lastExportFolder_;
    bool openFolderAfterExport_ = false;
    // Worker de ajustes del visor derecho: el denoise no debe congelar la UI.
    AdjustWorker* videoAdjustWorker_ = nullptr;
    // Mapa de píxeles calientes detectado sobre el vídeo abierto (CV_8U, tamaño
    // del frame original). Se recalcula solo si el usuario lo pide y se descarta
    // al abrir otro vídeo: las posiciones no valen para otro fichero.
    cv::Mat videoHotMask_;
    HotPixelScanWorker* hotScanWorker_ = nullptr;
    int64_t currentFrameIndex_ = 0;

    std::unique_ptr<IVideoReader> reader_;
    QString inPath_;
    int64_t currentUs_ = 0;
    int64_t stepUs_ = 40000;
    int64_t totalUs_ = 0;
    int64_t totalFrames_ = 0;
    // Info del fichero de vídeo (cacheada al abrir) para el panel Información.
    QString videoFileName_;
    QString videoFilePath_;
    QString videoFileDate_;
    qint64 videoFileSize_ = 0;
    QRect roi_;
    // Último frame mostrado (para "Ajustar fotograma": detectar el disco sin
    // re-leer el vídeo) y muestras del seguimiento del análisis por frame.
    cv::Mat currentFrameImage_;
    QVector<TrackSample> trackSamples_;
    // Círculo semilla de "Ajustar fotograma" (visible hasta que el análisis
    // genere muestras reales).
    QPointF seedCenter_{0.f, 0.f};
    float seedRadius_ = 0.f;
    bool hasSeed_ = false;

    PipelineWorker* worker_ = nullptr;
    VideoExportWorker* videoExportWorker_ = nullptr;
    std::vector<cv::Point2f> offsets_;
    // Último índice mostrado en el visor durante la exportación de vídeo, para
    // avanzar el lector de forma secuencial (sin re-buscar cada fotograma).
    int64_t lastShownExportIndex_ = -1;
    bool previewEnabled_ = false;
    int64_t startIndex_ = 0;
    int64_t startUs_ = 0;
};
