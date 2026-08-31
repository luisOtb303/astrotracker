#pragma once

#include "common/CircleF.h"
#include "motion/TrackStatus.h"
#include "stills/PhotoProject.h"
#include "stills/PhotoSequenceReader.h"
#include "tracking/DiscTracker.h"

#include <QWidget>
#include <map>
#include <vector>

class QAction;
class QComboBox;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QSlider;
class VideoView;
class PhotoTrackWorker;
class PhotoExportWorker;
class PhotoFrameLoader;
class PhotoFilmstrip;

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
    bool isBusy() const;

    // Estado persistible de la secuencia (para el proyecto .atracker).
    PhotoProjectPhotos collectState() const;
    // Restaura el trabajo guardado; devuelve false y rellena error si no pudo.
    bool applyState(const PhotoProjectPhotos& data, QString* error = nullptr);
    // Cierra la secuencia y limpia todos los resultados.
    void clearSession();

    // Perfil de seguimiento del proyecto y override de método por foto.
    TrackingProfile trackingProfile() const { return trackingProfile_; }
    void setTrackingProfile(ObjectProfile profile);
    DiscMethod overrideFor(int64_t index) const;
    void setOverrideForCurrent(DiscMethod method); // Prediction = quitar
    QString photoStatusText() const;

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
    // El trabajo ha cambiado (círculo, bloqueo, resultado del cálculo,
    // selección de exportación...): el proyecto debe marcarse como modificado.
    void modified();
    // El perfil de seguimiento del proyecto cambió (para sincronizar el panel).
    void profileChanged(ObjectProfile profile);
    // Texto de estado de la foto actual ("arco · 82% · válida") para el dock.
    void photoStatusChanged(const QString& statusText);
    // EXIF de la foto actual cambió (cuando se selecciona otra foto).
    void photoExifChanged(const PhotoExifInfo& exif);

private slots:
    void onItemActivated(QListWidgetItem* item);
    void onSliderChanged(int value);
    void showPrev();
    void showNext();
    void showCurrent();
    void onRoiSelected(const QRect& rect);
    void onCircleSelected(const QPointF& center, double radius);
    void onFitDisc();
    void onLockToggle(bool locked);
    void startExportVideo();
    void startExportPhotos();
    void runExportDialog(bool toVideo);
    void startNew();
    void onFilmstripChanged(QListWidgetItem* item);
    void onPhotoProcessed(int64_t index);
    void onFrameReady(int64_t index, const cv::Mat& frame);
    void setDrawModeCircle(bool circle);
    void runTracking();
    void stopTracking();
    void onWorkerProgress(int done, int total);
    void onWorkerReacquired(int64_t index, int predictedBefore);
    void onWorkerFinished(bool ok, const QString& error, const QVector<double>& results);
    void onExportFinished(bool ok, const QString& error, int frames);

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
    void updateFilmstripBadges();
    void refreshThumbnailCircles();
    void showCurrentSync();
    void ensureLoader();
    void updateViewerCircles(const cv::Mat& frame);

    cv::Mat centeredFrame(const cv::Mat& frame, const DiscTrack& track);
    cv::Mat centeredFrame(const cv::Mat& frame, const CircleF& circle);
    static QPixmap toPixmap(const cv::Mat& bgr);

    PhotoSequenceReader reader_;
    PhotoFilmstrip* filmstrip_ = nullptr;
    VideoView* view_ = nullptr;
    VideoView* resultView_ = nullptr;
    QSlider* slider_ = nullptr;
    QLabel* indexLabel_ = nullptr;
    QLabel* infoLabel_ = nullptr;
    QAction* prevAction_ = nullptr;
    QAction* nextAction_ = nullptr;
    QAction* analyzeAction_ = nullptr;
    QAction* fitAction_ = nullptr;
    QAction* exportVideoAction_ = nullptr;
    QAction* exportPhotosAction_ = nullptr;
    QAction* circleModeAction_ = nullptr;
    QAction* stopAction_ = nullptr;
    QAction* lockAction_ = nullptr;
    QAction* resetAction_ = nullptr;
    QComboBox* borderCombo_ = nullptr;

    bool drawCircleMode_ = true;
    bool trackingBusy_ = false;
    // Mientras se decodifica la foto actual, la edición del círculo/ROI está
    // deshabilitada para no capturar coordenadas de la miniatura (espacio
    // equivocado); solo se edita sobre el frame a resolución completa.
    bool loadingView_ = false;
    // Fotos bloqueadas: "Calcular automáticamente" no modifica su círculo.
    std::vector<bool> locked_;
    // Fotos corregidas a mano: el recálculo automático tampoco las modifica.
    std::vector<bool> manualFixed_;
    // Selección de exportación (espejo de las casillas del filmstrip) para
    // detectar cambios del usuario sin reaccionar a los refrescos de etiquetas.
    std::vector<bool> exportSelection_;
    // Origen de la secuencia actual, para guardarla en el proyecto.
    QString sourceFolder_;
    QStringList sourceFiles_;
    // Perfil de seguimiento y overrides de método por foto (dock Seguimiento).
    TrackingProfile trackingProfile_{trackingProfileFor(ObjectProfile::Auto)};
    std::map<int64_t, DiscMethod> methodOverrides_;
    // Miniaturas base del filmstrip (para el placeholder durante la carga y
    // para dibujar encima el círculo de cada foto).
    std::vector<cv::Mat> baseThumbs_;
    // Evita que la actualización de las etiquetas del filmstrip dispare el
    // re-sincronizado de la selección de exportación.
    bool updatingBadges_ = false;
    CircleF seedCircle_;
    bool hasSeedCircle_ = false;
    int64_t seedIndex_ = 0;
    std::vector<DiscTrack> tracks_;
    bool analyzed_ = false;
    PhotoTrackWorker* worker_ = nullptr;
    PhotoExportWorker* exportWorker_ = nullptr;
    PhotoFrameLoader* loader_ = nullptr;

    int64_t current_ = 0;
    int displayMaxDim_ = 1600;
    int thumbMaxDim_ = 240;
    QStringList recentFolders_;
    QString lastDir_;
};