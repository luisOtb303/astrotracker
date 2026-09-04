#include "ui/PhotoPanel.h"

#include "common/AppLog.h"
#include "ui/DebugDep.h"
#include "processing/BorderHandler.h"
#include "stills/PhotoExportWorker.h"
#include "stills/PhotoFrameLoader.h"
#include "stills/PhotoTrackWorker.h"
#include "tracking/DiscArcFit.h"
#include "ui/PhotoFilmstrip.h"
#include "ui/VideoView.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QBrush>
#include <QColor>
#include <QSettings>
#include <QSlider>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

QPixmap matToPixmap(const cv::Mat& bgr)
{
    if (bgr.empty())
        return QPixmap();
    cv::Mat rgb;
    cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
    const QImage img(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step),
                     QImage::Format_RGB888);
    return QPixmap::fromImage(img);
}

// Dibuja un círculo (continuo o discontinuo) sobre una miniatura BGR.
void drawCircleOn(cv::Mat& bgr, const cv::Point2f& c, int r, const cv::Scalar& color,
                  bool dashed)
{
    if (dashed) {
        const int n = 24;
        for (int k = 0; k < n; k += 2) {
            const double a0 = k * 2.0 * CV_PI / n;
            const double a1 = (k + 1) * 2.0 * CV_PI / n;
            const cv::Point p0(static_cast<int>(c.x + r * std::cos(a0)),
                               static_cast<int>(c.y + r * std::sin(a0)));
            const cv::Point p1(static_cast<int>(c.x + r * std::cos(a1)),
                               static_cast<int>(c.y + r * std::sin(a1)));
            cv::line(bgr, p0, p1, color, 1, cv::LINE_AA);
        }
    } else {
        cv::circle(bgr, cv::Point(static_cast<int>(c.x), static_cast<int>(c.y)), r, color, 1,
                   cv::LINE_AA);
    }
}

// Cadena legible de la prioridad de un perfil (ej. "Template → Arco").
QString profileMethodList(ObjectProfile profile)
{
    const auto methods = trackingProfileFor(profile).priority;
    QString result;
    for (size_t i = 0; i < methods.size(); ++i) {
        if (i > 0) result += QStringLiteral(" \xE2\x86\x92 ");  // →
        result += QString::fromUtf8(profileMethodName(methods[i]));
    }
    return result;
}

} // namespace

PhotoPanel::PhotoPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);

    QToolBar* tb = new QToolBar(this);
    tb->setMovable(false);
    tb->setToolButtonStyle(Qt::ToolButtonTextOnly);

    tb->addAction(tr("Abrir carpeta"), this, &PhotoPanel::openFolderDialog);
    tb->addAction(tr("Abrir fotos..."), this, &PhotoPanel::openImagesDialog);
    resetAction_ = tb->addAction(tr("Nuevo"));
    resetAction_->setEnabled(false);
    resetAction_->setToolTip(tr("Empezar de nuevo: cerrar la secuencia y limpiar los resultados"));
    connect(resetAction_, &QAction::triggered, this, &PhotoPanel::startNew);
    tb->addSeparator();

    QActionGroup* modeGroup = new QActionGroup(tb);
    modeGroup->setExclusive(true);
    circleModeAction_ = tb->addAction(tr("Círculo"));
    circleModeAction_->setCheckable(true);
    circleModeAction_->setChecked(true);
    circleModeAction_->setToolTip(tr("Dibujar/editar el círculo (posición del disco)"));
    modeGroup->addAction(circleModeAction_);
    QAction* rectModeAction = tb->addAction(tr("Rectángulo"));
    rectModeAction->setCheckable(true);
    rectModeAction->setToolTip(tr("Dibujar un recuadro; el círculo se ajusta dentro"));
    modeGroup->addAction(rectModeAction);
    connect(circleModeAction_, &QAction::toggled, this, &PhotoPanel::setDrawModeCircle);

    borderCombo_ = new QComboBox(this);
    borderCombo_->addItem(tr("Borde negro"));
    borderCombo_->addItem(tr("Borde réplica"));
    borderCombo_->setToolTip(tr("Relleno de los bordes al centrar el visor"));
    tb->addWidget(borderCombo_);
    connect(borderCombo_, &QComboBox::currentIndexChanged, this, [this](int) { emit modified(); });

    tb->addSeparator();
    prevAction_ = tb->addAction(tr("Anterior"), this, &PhotoPanel::showPrev);
    nextAction_ = tb->addAction(tr("Siguiente"), this, &PhotoPanel::showNext);
    tb->addSeparator();

    // Vista previa del timelapse: reproducción en bucle sobre las imágenes de
    // caché (_astrotracker_cache) de los RAW, con selector de fps.
    playAction_ = tb->addAction(tr("Reproducir TL"));
    playAction_->setCheckable(true);
    playAction_->setEnabled(false);
    playAction_->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    playAction_->setToolTip(tr("Reproducir la secuencia en bucle (vista previa "
                               "rápida del timelapse usando las miniaturas de caché)"));
    connect(playAction_, &QAction::toggled, this, &PhotoPanel::togglePlayback);
    fpsCombo_ = new QComboBox(this);
    fpsCombo_->setEnabled(false);
    fpsCombo_->setToolTip(tr("Fotogramas por segundo de la vista previa"));
    for (const int f : {1, 2, 5, 10, 15, 20, 25, 30, 60})
        fpsCombo_->addItem(tr("%1 fps").arg(f), f);
    fpsCombo_->setCurrentIndex(2); // 5 fps por defecto
    connect(fpsCombo_, &QComboBox::currentIndexChanged, this, [this](int) {
        if (playbackTimer_ && playbackTimer_->isActive())
            playbackTimer_->setInterval(1000 / fpsCombo_->currentData().toInt());
        emit photoPositionChanged(current_);
    });
    tb->addWidget(fpsCombo_);

    playbackTimer_ = new QTimer(this);
    playbackTimer_->setTimerType(Qt::PreciseTimer);
    connect(playbackTimer_, &QTimer::timeout, this, &PhotoPanel::onPlaybackTick);

    tb->addSeparator();
    analyzeAction_ = tb->addAction(tr("Calcular automáticamente"));
    analyzeAction_->setEnabled(false);
    analyzeAction_->setToolTip(tr("Seguir el disco en todas las fotos. Si no hay "
                                   "círculo semilla, se detectará automáticamente "
                                   "en la foto actual. Las fotos bloqueadas no se "
                                   "modifican."));
    connect(analyzeAction_, &QAction::triggered, this, &PhotoPanel::runTracking);
    fitAction_ = tb->addAction(tr("Ajustar fotograma"));
    fitAction_->setEnabled(false);
    fitAction_->setToolTip(tr("Detectar el disco solo en la foto actual: ajusta el "
                              "círculo que forma la fase visible (parcial, creciente o "
                              "corona)"));
    connect(fitAction_, &QAction::triggered, this, &PhotoPanel::onFitDisc);
    exportVideoAction_ = tb->addAction(tr("Exportar vídeo..."));
    exportVideoAction_->setEnabled(false);
    exportVideoAction_->setToolTip(tr("Guardar la secuencia como vídeo MP4 (centrada si se "
                                      "ha seguido, directa si no)"));
    connect(exportVideoAction_, &QAction::triggered, this, &PhotoPanel::startExportVideo);
    exportPhotosAction_ = tb->addAction(tr("Exportar fotos..."));
    exportPhotosAction_->setEnabled(false);
    exportPhotosAction_->setToolTip(tr("Guardar la secuencia como imágenes PNG/JPG (centradas "
                                       "si se ha seguido, directas si no)"));
    connect(exportPhotosAction_, &QAction::triggered, this, &PhotoPanel::startExportPhotos);
    stopAction_ = tb->addAction(tr("Detener"));
    stopAction_->setEnabled(false);
    stopAction_->setToolTip(tr("Detener el seguimiento o la exportación en curso"));
    connect(stopAction_, &QAction::triggered, this, &PhotoPanel::stopTracking);
    lockAction_ = tb->addAction(tr("Bloquear fotograma"));
    lockAction_->setCheckable(true);
    lockAction_->setEnabled(false);
    lockAction_->setToolTip(tr("Fijar el círculo de la foto actual: \"Calcular "
                               "automáticamente\" no lo modificará"));
    connect(lockAction_, &QAction::toggled, this, &PhotoPanel::onLockToggle);
    root->addWidget(tb);

    auto* viewers = new QHBoxLayout();

    auto* sourceBox = new QVBoxLayout();
    auto* srcTitle = new QLabel(tr("Original"), this);
    srcTitle->setAlignment(Qt::AlignCenter);
    sourceBox->addWidget(srcTitle);
    view_ = new VideoView(this);
    sourceBox->addWidget(view_, 1);

    auto* resultBox = new QVBoxLayout();
    auto* resTitle = new QLabel(tr("Centrado"), this);
    resTitle->setAlignment(Qt::AlignCenter);
    resultBox->addWidget(resTitle);
    resultView_ = new VideoView(this);
    resultView_->setRoiEnabled(false);
    resultView_->setCircleEnabled(false);
    resultBox->addWidget(resultView_, 1);

    viewers->addLayout(sourceBox, 1);
    viewers->addLayout(resultBox, 1);
    root->addLayout(viewers, 1);

    slider_ = new QSlider(Qt::Horizontal, this);
    slider_->setEnabled(false);
    root->addWidget(slider_);

    auto* bottom = new QHBoxLayout();
    indexLabel_ = new QLabel(tr("Foto: - / -"), this);
    infoLabel_ = new QLabel(tr("Abrir una carpeta o seleccionar fotos para empezar"), this);
    infoLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    bottom->addWidget(indexLabel_, 1);
    bottom->addWidget(infoLabel_);
    root->addLayout(bottom);

    filmstrip_ = new PhotoFilmstrip(this);
    filmstrip_->setViewMode(QListView::IconMode);
    filmstrip_->setMovement(QListView::Static);
    filmstrip_->setResizeMode(QListView::Adjust);
    filmstrip_->setWrapping(false);
    filmstrip_->setUniformItemSizes(false);
    filmstrip_->setIconSize(QSize(120, 80));
    filmstrip_->setGridSize(QSize(140, 110));
    filmstrip_->setSpacing(4);
    filmstrip_->setFlow(QListView::LeftToRight);
    filmstrip_->setSelectionMode(QAbstractItemView::SingleSelection);
    filmstrip_->setMinimumHeight(135);
    root->addWidget(filmstrip_);

    connect(filmstrip_, &QListWidget::itemActivated, this, &PhotoPanel::onItemActivated);
    connect(filmstrip_, &QListWidget::itemClicked, this, &PhotoPanel::onItemActivated);
    connect(filmstrip_, &QListWidget::itemChanged, this, &PhotoPanel::onFilmstripChanged);
    connect(slider_, &QSlider::valueChanged, this, &PhotoPanel::onSliderChanged);
    connect(view_, &VideoView::roiSelected, this, &PhotoPanel::onRoiSelected);
    connect(view_, &VideoView::circleSelected, this, &PhotoPanel::onCircleSelected);

    prevAction_->setEnabled(false);
    nextAction_->setEnabled(false);
    applyViewModes();

    QSettings settings;
    recentFolders_ = settings.value("Photos/recentFolders").toStringList();
    lastDir_ = settings.value("Photos/lastDir").toString();
    ObjectProfile saved = ObjectProfile::Auto;
    if (objectProfileFromKey(
            settings.value("Photos/profile").toString().toLatin1().constData(),
            saved))
        trackingProfile_ = trackingProfileFor(saved);
}

PhotoPanel::~PhotoPanel() = default;

void PhotoPanel::openImagesDialog()
{
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, tr("Seleccionar fotos (secuencia)"), dialogStartDir(),
        tr("Imágenes (*.jpg *.jpeg *.png *.tif *.tiff *.bmp *.cr2 *.cr3 *.dng *.nef);;"
           "RAW (*.cr2 *.cr3 *.dng *.nef *.arw *.orf *.raf *.rw2 *.pef *.srw *.raw);;"
           "Todos los archivos (*.*)"));
    if (paths.isEmpty())
        return;
    openPaths(paths);
}

void PhotoPanel::openFolderDialog()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Abrir carpeta de fotos"),
                                                          dialogStartDir());
    if (dir.isEmpty())
        return;
    openFolder(dir);
}

void PhotoPanel::openRecentFolder(const QString& dir)
{
    if (dir.isEmpty())
        return;
    openFolder(dir);
}

void PhotoPanel::openFolder(const QString& dir)
{
    emit statusMessage(tr("Leyendo la carpeta de fotos..."));
    const bool ok = reader_.openFolder(dir.toStdString());
    if (!ok) {
        emit statusMessage(tr("La carpeta no contiene imágenes soportadas: %1").arg(dir), 5000);
        QMessageBox::warning(this, tr("AstroTracker"),
                             tr("La carpeta no contiene imágenes soportadas:\n%1").arg(dir));
        return;
    }
    sourceFolder_ = QDir(dir).absolutePath();
    sourceFiles_.clear();
    rememberFolder(dir);
    reloadSequence();
    AppLog::info(tr("Carpeta abierta: %1 (%2 fotos)").arg(dir).arg(reader_.count()));
}

void PhotoPanel::rememberFolder(const QString& dir)
{
    if (dir.isEmpty())
        return;
    const QString norm = QDir(dir).absolutePath();
    recentFolders_.removeAll(norm);
    recentFolders_.push_front(norm);
    while (recentFolders_.size() > 8)
        recentFolders_.removeLast();

    QSettings settings;
    settings.setValue("Photos/recentFolders", recentFolders_);
    lastDir_ = norm;
    settings.setValue("Photos/lastDir", lastDir_);
}

QString PhotoPanel::dialogStartDir() const
{
    if (!lastDir_.isEmpty() && QDir(lastDir_).exists())
        return lastDir_;
    return QString();
}

void PhotoPanel::openPaths(const QStringList& paths)
{
    std::vector<std::string> v;
    v.reserve(static_cast<size_t>(paths.size()));
    for (const QString& p : paths)
        v.push_back(p.toStdString());

    emit statusMessage(tr("Abriendo %1 fotos...").arg(paths.size()));
    const bool ok = reader_.open(v);
    if (!ok) {
        emit statusMessage(tr("Ninguno de los archivos es una imagen soportada"), 5000);
        QMessageBox::warning(this, tr("AstroTracker"),
                             tr("Ninguno de los archivos seleccionados es una imagen soportada."));
        return;
    }
    sourceFolder_.clear();
    sourceFiles_ = paths;
    rememberFolder(QFileInfo(paths.first()).absolutePath());
    reloadSequence();
    AppLog::info(tr("Secuencia abierta: %1 fotos").arg(paths.size()));
}

void PhotoPanel::clearSession()
{
    reader_.close();
    if (loader_) {
        loader_->shutdown();
        loader_ = nullptr;
    }
    current_ = 0;
    filmstrip_->clear();
    baseThumbs_.clear();
    manualFixed_.clear();
    exportSelection_.clear();
    sourceFolder_.clear();
    sourceFiles_.clear();
    view_->setFrame(cv::Mat());
    resultView_->setFrame(cv::Mat());
    view_->clearCircle();
    resultView_->clearCircle();
    slider_->setEnabled(false);
    prevAction_->setEnabled(false);
    nextAction_->setEnabled(false);
    hasSeedCircle_ = false;
    seedIndex_ = 0;
    tracks_.clear();
    analyzed_ = false;
    locked_.clear();
    methodOverrides_.clear();
    loadingView_ = false;
    updatingBadges_ = false;
    if (lockAction_)
        lockAction_->setChecked(false);
    indexLabel_->setText(tr("Foto: - / -"));
    infoLabel_->setText(tr("Abrir una carpeta o seleccionar fotos para empezar"));
    updateTrackingUi();
}

void PhotoPanel::reloadSequence()
{
    current_ = 0;
    filmstrip_->clear();
    baseThumbs_.assign(static_cast<size_t>(reader_.count()), cv::Mat());
    manualFixed_.clear();
    exportSelection_.clear();
    buildFilmstrip();
    ensureLoader();

    slider_->setRange(0, static_cast<int>(reader_.count() - 1));
    slider_->setValue(0);
    slider_->setEnabled(reader_.count() > 1);
    prevAction_->setEnabled(false);
    nextAction_->setEnabled(reader_.count() > 1);
    if (filmstrip_->count() > 0)
        filmstrip_->setCurrentRow(0);

    hasSeedCircle_ = false;
    seedIndex_ = 0;
    tracks_.clear();
    analyzed_ = false;
    locked_.clear();
    if (lockAction_)
        lockAction_->setChecked(false);
    view_->clearCircle();
    resultView_->clearCircle();

    showCurrent();
    updateNavUi();
    updateTrackingUi();
}

void PhotoPanel::buildFilmstrip()
{
    const int64_t total = reader_.count();
    exportSelection_.assign(static_cast<size_t>(total), true);
    emit workProgress(0, static_cast<int>(total));
    emit statusMessage(tr("Generando miniaturas 0/%1...").arg(total));

    for (int64_t i = 0; i < total; ++i) {
        cv::Mat thumb;
        if (!reader_.thumbnail(i, thumb, thumbMaxDim_))
            continue;
        if (static_cast<size_t>(i) < baseThumbs_.size())
            baseThumbs_[static_cast<size_t>(i)] = thumb;
        auto* item = new QListWidgetItem(QIcon(matToPixmap(thumb)), QString());
        item->setToolTip(QString::fromStdString(reader_.fileName(i)));
        item->setData(Qt::UserRole, static_cast<qlonglong>(i));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked); // exportada por defecto
        filmstrip_->addItem(item);

        emit workProgress(static_cast<int>(i + 1), static_cast<int>(total));
        emit statusMessage(tr("Generando miniaturas %1/%2...").arg(i + 1).arg(total));
        QApplication::processEvents();
    }

    emit workProgress(-1, -1);
    emit statusMessage(tr("%1 fotos cargadas").arg(total), 5000);
    updateFilmstripBadges();
}

void PhotoPanel::updateFilmstripBadges()
{
    // Etiqueta bajo cada miniatura: "válida"/"supuesta"/"dudosa" según el
    // resultado del seguimiento, coloreada para distinguirla de un vistazo.
    for (int i = 0; i < filmstrip_->count(); ++i) {
        QListWidgetItem* item = filmstrip_->item(i);
        const int64_t idx = item->data(Qt::UserRole).toLongLong();
        if (idx < 0 || idx >= static_cast<int64_t>(tracks_.size())) {
            item->setText(QString());
            continue;
        }
        const DiscTrack& t = tracks_[static_cast<size_t>(idx)];
        QString label;
        QColor color;
        if (idx < static_cast<int64_t>(locked_.size()) && locked_[static_cast<size_t>(idx)]) {
            label = tr("bloqueada");
            color = QColor(0x1e, 0x90, 0xff);
        } else if (t.predicted) {
            label = tr("supuesta");
            color = QColor(0xff, 0xa5, 0x00);
        } else if (t.status == TrackStatus::VALID) {
            label = tr("válida");
            color = QColor(0x2e, 0x8b, 0x57);
        } else {
            label = tr("dudosa");
            color = QColor(0xc0, 0xc0, 0xc0);
        }
        item->setText(label);
        item->setForeground(QBrush(color));
    }
    refreshThumbnailCircles();
}

void PhotoPanel::refreshThumbnailCircles()
{
    const double s = displayMaxDim_ > 0 ? static_cast<double>(thumbMaxDim_) / displayMaxDim_ : 1.0;
    for (int i = 0; i < filmstrip_->count(); ++i) {
        QListWidgetItem* item = filmstrip_->item(i);
        const int64_t idx = item->data(Qt::UserRole).toLongLong();
        if (idx < 0 || idx >= static_cast<int64_t>(baseThumbs_.size()))
            continue;
        cv::Mat thumb = baseThumbs_[static_cast<size_t>(idx)].clone();
        const bool hasTrack = analyzed_ && idx < static_cast<int64_t>(tracks_.size()) &&
                              tracks_[static_cast<size_t>(idx)].radius > 0.f;
        if (hasTrack) {
            const DiscTrack& t = tracks_[static_cast<size_t>(idx)];
            const cv::Point2f c(t.center.x * static_cast<float>(s),
                                t.center.y * static_cast<float>(s));
            const int r = std::max(1, static_cast<int>(t.radius * static_cast<float>(s)));
            drawCircleOn(thumb, c, r, cv::Scalar(0, 210, 0), t.predicted);
        }
        item->setIcon(QIcon(matToPixmap(thumb)));
    }
}

void PhotoPanel::onItemActivated(QListWidgetItem* item)
{
    if (!item)
        return;
    const int64_t idx = item->data(Qt::UserRole).toLongLong();
    if (idx == current_)
        return;
    current_ = idx;
    showCurrent();
    updateNavUi();
}

void PhotoPanel::showPhoto(int64_t index)
{
    if (!reader_.isOpen() || index < 0 || index >= reader_.count())
        return;
    if (playbackTimer_ && playbackTimer_->isActive())
        playAction_->setChecked(false); // parar el preview al navegar
    if (index == current_)
        return;
    current_ = index;
    showCurrent();
    updateNavUi();
}

void PhotoPanel::onSliderChanged(int value)
{
    if (!reader_.isOpen() || value == static_cast<int>(current_))
        return;
    current_ = value;
    showCurrent();
    updateNavUi();
}

void PhotoPanel::showPrev()
{
    if (!reader_.isOpen() || current_ <= 0)
        return;
    --current_;
    showCurrent();
    updateNavUi();
}

void PhotoPanel::showNext()
{
    if (!reader_.isOpen() || current_ >= reader_.count() - 1)
        return;
    ++current_;
    showCurrent();
    updateNavUi();
}

void PhotoPanel::showCurrentSync()
{
    loadingView_ = false;
    applyViewModes();
    cv::Mat frame;
    if (!reader_.readAt(current_, frame, displayMaxDim_)) {
        view_->setFrame(cv::Mat());
        resultView_->setFrame(cv::Mat());
        return;
    }
    view_->setFrame(frame);
    updateViewerCircles(frame);

    emitFileInfo(current_);
}

void PhotoPanel::togglePlayback(bool checked)
{
    if (checked && (!reader_.isOpen() || reader_.count() < 2 || isBusy())) {
        playAction_->setChecked(false);
        return;
    }
    if (checked) {
        if (reader_.count() < 2)
            return;
        // El preview rota por las imágenes de caché (rápido); se bloquea la
        // navegación mientras reproduce para no desincronizar el bucle.
        playAction_->setText(tr("Pausa"));
        playAction_->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
        playbackTimer_->setInterval(1000 / fpsCombo_->currentData().toInt());
        playbackTimer_->start();
    } else {
        playAction_->setText(tr("Reproducir TL"));
        playAction_->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        playbackTimer_->stop();
        // Al pausar se restaura la vista normal a resolución completa.
        showCurrent();
    }
}

void PhotoPanel::onPlaybackTick()
{
    if (!reader_.isOpen() || reader_.count() == 0)
        return;
    if (reader_.count() < 2) {
        togglePlayback(false);
        return;
    }
    int64_t next = current_ + 1;
    if (next >= reader_.count())
        next = 0; // bucle
    current_ = next;

    // readAt con displayMaxDim_ usa la caché de análisis (_astrotracker_cache)
    // para los RAW: leer y centrar es barato, idóneo para el preview en bucle.
    cv::Mat frame;
    if (reader_.readAt(current_, frame, displayMaxDim_) && !frame.empty()) {
        view_->setLoading(false);
        resultView_->setLoading(false);
        view_->setFrame(frame);
        updateViewerCircles(frame);
    }
    updateNavUi();
    emitFileInfo(current_);
}

void PhotoPanel::emitFileInfo(int64_t index)
{
    if (index < 0 || index >= reader_.count())
        return;
    const QString path = QString::fromStdString(reader_.filePath(index));
    const QFileInfo fi(path);
    PhotoFileInfo info;
    info.name = fi.fileName().toStdString();
    info.path = fi.absoluteFilePath().toStdString();
    info.sizeBytes = fi.size();
    info.modifyDate = fi.lastModified().toString(Qt::ISODate).toStdString();
    info.width = reader_.width();
    info.height = reader_.height();
    const QString ext = fi.suffix().toUpper();
    info.type = ext.isEmpty() ? QStringLiteral("Desconocido").toStdString()
                              : ext.toStdString();
    emit photoMetaChanged(info, reader_.exifInfo(index));
    depLog(QStringLiteral("emitFileInfo idx=%1 count=%2 name=%3")
               .arg(index).arg(reader_.count()).arg(QString::fromStdString(info.name)));
}

void PhotoPanel::showCurrent()
{
    if (!reader_.isOpen())
        return;
    // La info del fichero/EXIF se actualiza siempre al cambiar de foto, tanto
    // por la ruta síncrona (sin loader) como por la asíncrona (el loader no
    // re-emite frameReady para índices ya en caché).
    emitFileInfo(current_);
    if (!loader_ || !loader_->isRunning()) {
        showCurrentSync();
        return;
    }

    // Mientras se decodifica el frame (los RAW tardan) se muestra la miniatura
    // con un indicador "Abriendo foto…" y la edición queda deshabilitada.
    loadingView_ = true;
    applyViewModes();
    const double s = displayMaxDim_ > 0 ? static_cast<double>(thumbMaxDim_) / displayMaxDim_ : 1.0;
    cv::Mat thumb;
    if (current_ < static_cast<int64_t>(baseThumbs_.size()))
        thumb = baseThumbs_[static_cast<size_t>(current_)];
    if (thumb.empty()) {
        view_->setFrame(cv::Mat());
        resultView_->setFrame(cv::Mat());
    } else {
        view_->setFrame(thumb);
        resultView_->setFrame(thumb);
    }
    if (analyzed_ && current_ < static_cast<int64_t>(tracks_.size())) {
        const DiscTrack& t = tracks_[static_cast<size_t>(current_)];
        if (t.radius > 0.f) {
            view_->setCircle(QPointF(t.center.x * s, t.center.y * s), t.radius * s, t.predicted);
            resultView_->setCircle(QPointF(thumb.cols / 2.0, thumb.rows / 2.0),
                                   t.radius * s, t.predicted);
        } else {
            view_->clearCircle();
            resultView_->clearCircle();
        }
    } else if (hasSeedCircle_) {
        view_->setCircle(QPointF(seedCircle_.center.x * s, seedCircle_.center.y * s),
                         seedCircle_.radius * s, false);
        resultView_->clearCircle();
    } else {
        view_->clearCircle();
        resultView_->clearCircle();
    }

    view_->setLoading(true);
    resultView_->setLoading(true);
    loader_->requestLoad(current_);
}

void PhotoPanel::updateViewerCircles(const cv::Mat& frame)
{
    if (analyzed_ && current_ < static_cast<int64_t>(tracks_.size())) {
        const DiscTrack& t = tracks_[static_cast<size_t>(current_)];
        if (t.radius > 0.f) {
            view_->setCircle(QPointF(t.center.x, t.center.y), t.radius, t.predicted);
            resultView_->setFrame(centeredFrame(frame, t));
            resultView_->setCircle(QPointF(frame.cols / 2.0, frame.rows / 2.0),
                                   t.radius, t.predicted);
            return;
        }
    }

    if (hasSeedCircle_) {
        view_->setCircle(QPointF(seedCircle_.center.x, seedCircle_.center.y),
                         seedCircle_.radius, false);
        resultView_->setFrame(centeredFrame(frame, seedCircle_));
        resultView_->clearCircle();
    } else {
        resultView_->setFrame(frame);
        resultView_->clearCircle();
    }
}

void PhotoPanel::ensureLoader()
{
    if (loader_ && loader_->isRunning())
        return;
    QStringList paths;
    paths.reserve(static_cast<int>(reader_.count()));
    for (int64_t i = 0; i < reader_.count(); ++i)
        paths.push_back(QString::fromStdString(reader_.filePath(i)));
    loader_ = new PhotoFrameLoader(paths, displayMaxDim_, this);
    connect(loader_, &PhotoFrameLoader::frameReady, this, &PhotoPanel::onFrameReady);
    connect(loader_, &QThread::finished, loader_, &QObject::deleteLater);
    loader_->start();
}

void PhotoPanel::updateNavUi()
{
    slider_->blockSignals(true);
    slider_->setValue(static_cast<int>(current_));
    slider_->blockSignals(false);

    if (filmstrip_ && filmstrip_->count() > 0 && current_ < filmstrip_->count())
        filmstrip_->setCurrentRow(static_cast<int>(current_));

    prevAction_->setEnabled(current_ > 0);
    nextAction_->setEnabled(current_ < reader_.count() - 1);
    indexLabel_->setText(tr("Foto: %1 / %2")
                             .arg(current_ + 1)
                             .arg(reader_.count()));
    QString info = QString::fromStdString(reader_.fileName(current_));
    if (analyzed_ && current_ < static_cast<int64_t>(tracks_.size())) {
        const DiscTrack& t = tracks_[static_cast<size_t>(current_)];
        if (current_ < static_cast<int64_t>(locked_.size()) &&
            locked_[static_cast<size_t>(current_)])
            info += tr("  ·  bloqueada");
        else if (t.predicted)
            info += tr("  ·  círculo supuesto");
        else if (t.status == TrackStatus::VALID)
            info += tr("  ·  válido");
        else if (t.status == TrackStatus::UNCERTAIN)
            info += tr("  ·  incierto");
    }
    if (lockAction_) {
        lockAction_->setChecked(current_ < static_cast<int64_t>(locked_.size()) &&
                               locked_[static_cast<size_t>(current_)]);
    }
    infoLabel_->setText(info);
    emit photoStatusChanged(photoStatusText());
    emit photoPositionChanged(current_);

    // Vista previa del timelapse: solo con 2+ fotos y sin trabajo en curso.
    const bool canPlay = reader_.isOpen() && reader_.count() >= 2 && !isBusy();
    if (playAction_) {
        playAction_->setEnabled(canPlay);
        if (fpsCombo_)
            fpsCombo_->setEnabled(canPlay);
        if (!canPlay && playbackTimer_ && playbackTimer_->isActive())
            playAction_->setChecked(false);
    }
}

void PhotoPanel::onRoiSelected(const QRect& rect)
{
    if (rect.isEmpty())
        return;
    const cv::Point2f center(rect.x() + rect.width() / 2.0f,
                             rect.y() + rect.height() / 2.0f);
    const float radius = std::min(rect.width(), rect.height()) / 2.0f;
    applyCircle(center, radius);
}

void PhotoPanel::onCircleSelected(const QPointF& center, double radius)
{
    if (radius <= 0.0)
        return;
    applyCircle(cv::Point2f(static_cast<float>(center.x()),
                            static_cast<float>(center.y())),
                static_cast<float>(radius));
}

void PhotoPanel::setDrawModeCircle(bool circle)
{
    drawCircleMode_ = circle;
    applyViewModes();
}

void PhotoPanel::applyCircle(const cv::Point2f& center, float radius)
{
    if (radius <= 0.f)
        return;
    seedCircle_ = CircleF{center, radius};
    hasSeedCircle_ = true;
    seedIndex_ = current_;
    view_->setCircle(QPointF(center.x, center.y), radius, false);

    if (analyzed_ && !trackingBusy_ && current_ < static_cast<int64_t>(tracks_.size())) {
        // Edición manual: solo se actualiza esta foto, sin relanzar nada. La
        // foto queda "fijada": el recálculo automático no la sobrescribirá.
        DiscTrack& t = tracks_[static_cast<size_t>(current_)];
        t.center = center;
        t.radius = radius;
        t.status = TrackStatus::VALID;
        t.predicted = false;
        if (manualFixed_.size() <= static_cast<size_t>(current_))
            manualFixed_.resize(static_cast<size_t>(current_) + 1, false);
        manualFixed_[static_cast<size_t>(current_)] = true;
        updateFilmstripBadges();
        showCurrent();
        updateNavUi();
        emit statusMessage(
            tr("Círculo ajustado en la foto %1 (fijada: el cálculo automático ya "
               "no la modificará).")
                .arg(current_ + 1));
        AppLog::info(tr("Círculo ajustado manualmente en la foto %1 (%2,%3 r%4)")
                         .arg(current_ + 1)
                         .arg(center.x, 0, 'f', 0)
                         .arg(center.y, 0, 'f', 0)
                         .arg(radius, 0, 'f', 0));
    } else {
        emit statusMessage(tr("Centrando la foto %1 con el círculo pintado...").arg(current_ + 1));
        showCurrent();
        updateTrackingUi();
        emit statusMessage(
            tr("Círculo aplicado en la foto %1. Pulsa \"Calcular automáticamente\" para "
               "centrar las %2 fotos.")
                .arg(current_ + 1)
                .arg(reader_.count()));
    }
    emit modified();
}

void PhotoPanel::startNew()
{
    if (trackingBusy_ || !reader_.isOpen())
        return;
    const bool hasWork = analyzed_ || hasSeedCircle_;
    if (hasWork) {
        const auto btn = QMessageBox::question(
            this, tr("AstroTracker"),
            tr("Empezar de nuevo: se cerrará la secuencia actual y se perderán "
               "los resultados. ¿Continuar?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (btn != QMessageBox::Yes)
            return;
    }
    AppLog::info(tr("Nueva sesión: cerrada la secuencia anterior"));
    clearSession();
}

void PhotoPanel::onLockToggle(bool locked)
{
    if (locked_.size() <= static_cast<size_t>(current_))
        locked_.resize(static_cast<size_t>(current_) + 1, false);
    locked_[static_cast<size_t>(current_)] = locked;
    updateFilmstripBadges();
    updateNavUi();
    AppLog::info(locked ? tr("Foto %1 bloqueada").arg(current_ + 1)
                        : tr("Foto %1 desbloqueada").arg(current_ + 1));
    emit statusMessage(locked ? tr("Foto %1 bloqueada: no se modificará en el cálculo "
                                    "automático.")
                                     .arg(current_ + 1)
                               : tr("Foto %1 desbloqueada.")
                                     .arg(current_ + 1));
    emit modified();
}

void PhotoPanel::applyViewModes()
{
    view_->setCircleEnabled(drawCircleMode_ && !trackingBusy_ && !loadingView_);
    view_->setRoiEnabled(!drawCircleMode_ && !trackingBusy_ && !loadingView_);
}

void PhotoPanel::onFitDisc()
{
    if (trackingBusy_ || !reader_.isOpen())
        return;
    cv::Mat fr;
    if (!reader_.readAt(current_, fr, displayMaxDim_) || fr.empty())
        return;
    cv::Mat g;
    cv::cvtColor(fr, g, cv::COLOR_BGR2GRAY);
    const cv::Rect full(0, 0, fr.cols, fr.rows);
    const float guess = hasSeedCircle_
                            ? seedCircle_.radius
                            : std::max(20.f, 0.10f * static_cast<float>(fr.cols));
    const DiscArcEstimate e = DiscArcFit::fitDisc(
        g, full, cv::Point2f(fr.cols * 0.5f, fr.rows * 0.5f), guess, 3.f);
    if (!e.ok || e.radius <= 0.f) {
        emit statusMessage(tr("No se ha podido ajustar el círculo del disco en esta foto"), 0);
        AppLog::error(tr("Ajuste del disco fallido en la foto %1").arg(current_ + 1));
        return;
    }
    AppLog::info(tr("Disco ajustado en la foto %1 (%2,%3 r%4, arco %5°)")
                     .arg(current_ + 1)
                     .arg(e.center.x, 0, 'f', 0)
                     .arg(e.center.y, 0, 'f', 0)
                     .arg(e.radius, 0, 'f', 0)
                     .arg(e.spanDeg, 0, 'f', 0));
    applyCircle(e.center, e.radius);
}

bool PhotoPanel::autoDetectSeed()
{
    cv::Mat fr;
    if (!reader_.readAt(current_, fr, displayMaxDim_) || fr.empty())
        return false;
    cv::Mat g;
    cv::cvtColor(fr, g, cv::COLOR_BGR2GRAY);
    const cv::Rect full(0, 0, g.cols, g.rows);
    const float guess = std::max(20.f, 0.10f * static_cast<float>(g.cols));
    const DiscArcEstimate e = DiscArcFit::fitDisc(
        g, full, cv::Point2f(g.cols * 0.5f, g.rows * 0.5f), guess, 3.f);
    if (!e.ok || e.radius <= 0.f)
        return false;
    applyCircle(e.center, e.radius);
    return true;
}

void PhotoPanel::startExportVideo()
{
    runExportDialog(true);
}

void PhotoPanel::startExportPhotos()
{
    runExportDialog(false);
}

void PhotoPanel::runExportDialog(bool toVideo)
{
    if (exportWorker_ || worker_ || !reader_.isOpen())
        return;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Exportar secuencia"));
    auto* lay = new QVBoxLayout(&dlg);

    // Centrado: qué se exporta ahora mismo. "Directo" siempre disponible
    // (TL plano); "Centrado" solo si hay resultado del seguimiento.
    auto* centringGroup = new QGroupBox(tr("Contenido"), &dlg);
    auto* centringLay = new QVBoxLayout(centringGroup);
    auto* directRadio = new QRadioButton(tr("Directo (sin centrar)"), &dlg);
    auto* centeredRadio = new QRadioButton(tr("Centrado (seguimiento)"), &dlg);
    const bool canCenter = analyzed_ && !tracks_.empty();
    directRadio->setToolTip(tr("Exporta las fotos tal cual han sido tomadas "
                               "(no se aplica ningún desplazamiento)"));
    centeredRadio->setToolTip(tr("Desplaza cada foto para dejar el disco fijo "
                                 "en el centro"));
    centeredRadio->setEnabled(canCenter);
    centeredRadio->setChecked(canCenter);
    if (canCenter)
        directRadio->setChecked(false);
    else
        directRadio->setChecked(true);
    centringLay->addWidget(directRadio);
    centringLay->addWidget(centeredRadio);
    lay->addWidget(centringGroup);

    auto* fmtLabel = new QLabel(tr("Formato"), &dlg);
    lay->addWidget(fmtLabel);
    auto* jpgRadio = new QRadioButton(tr("Fotos JPG"), &dlg);
    auto* pngRadio = new QRadioButton(tr("Fotos PNG"), &dlg);
    auto* mp4Radio = new QRadioButton(tr("Vídeo MP4"), &dlg);
    if (toVideo)
        mp4Radio->setChecked(true);
    else
        jpgRadio->setChecked(true);
    jpgRadio->setToolTip(tr("Una imagen por foto"));
    pngRadio->setToolTip(tr("Una imagen por foto (sin pérdida)"));
    mp4Radio->setToolTip(tr("Un vídeo H.264 (MP4) con todas las fotos"));
    lay->addWidget(jpgRadio);
    lay->addWidget(pngRadio);
    lay->addWidget(mp4Radio);

    auto* resLabel = new QLabel(tr("Resolución"), &dlg);
    lay->addWidget(resLabel);
    auto* resCombo = new QComboBox(&dlg);
    resCombo->addItem(tr("Original"),
                      static_cast<int>(PhotoExportWorker::Resolution::Original));
    resCombo->addItem(tr("Visor (1600 px)"),
                      static_cast<int>(PhotoExportWorker::Resolution::Visor));
    resCombo->addItem(tr("HD (1280×720)"),
                      static_cast<int>(PhotoExportWorker::Resolution::HD));
    resCombo->addItem(tr("FHD (1920×1080)"),
                      static_cast<int>(PhotoExportWorker::Resolution::FHD));
    resCombo->addItem(tr("2K (2560×1440)"),
                      static_cast<int>(PhotoExportWorker::Resolution::QHD));
    resCombo->addItem(tr("4K (3840×2160)"),
                      static_cast<int>(PhotoExportWorker::Resolution::UHD));
    resCombo->setCurrentIndex(3); // FHD
    lay->addWidget(resCombo);

    auto* fpsLabel = new QLabel(tr("FPS (solo vídeo)"), &dlg);
    lay->addWidget(fpsLabel);
    auto* fpsCombo = new QComboBox(&dlg);
    fpsCombo->setEditable(true);
    fpsCombo->addItems({QStringLiteral("10"), QStringLiteral("5"), QStringLiteral("24"),
                        QStringLiteral("30")});
    fpsCombo->setCurrentText(QStringLiteral("10"));
    lay->addWidget(fpsCombo);

    auto* smoothGroup = new QGroupBox(tr("Suavizar transiciones (solo vídeo)"), &dlg);
    auto* smoothLay = new QVBoxLayout(smoothGroup);
    auto* interpRow = new QHBoxLayout();
    auto* interpCombo = new QComboBox(&dlg);
    interpCombo->addItem(tr("Sin suavizado"), 0);
    interpCombo->addItem(tr("1 intermedio"), 1);
    interpCombo->addItem(tr("2 intermedios"), 2);
    interpCombo->addItem(tr("3 intermedios"), 3);
    interpCombo->addItem(tr("4 intermedios"), 4);
    interpCombo->setCurrentIndex(2);
    interpCombo->setToolTip(tr("Fotogramas generados entre cada par de fotos para "
                               "que la transición no sea brusca"));
    interpRow->addWidget(new QLabel(tr("Fotogramas intermedios"), &dlg));
    interpRow->addWidget(interpCombo, 1);
    smoothLay->addLayout(interpRow);
    auto* brightChk = new QCheckBox(tr("Normalizar brillo entre fotos"), &dlg);
    brightChk->setChecked(true);
    brightChk->setToolTip(tr("Escala el brillo de cada foto al de la primera para "
                             "evitar el parpadeo entre tomas"));
    smoothLay->addWidget(brightChk);
    lay->addWidget(smoothGroup);

    auto* destLabel = new QLabel(tr("Destino"), &dlg);
    lay->addWidget(destLabel);
    auto* destRow = new QHBoxLayout();
    auto* destEdit = new QLineEdit(&dlg);
    destEdit->setReadOnly(true);
    auto* browseBtn = new QPushButton(tr("Examinar..."), &dlg);
    destRow->addWidget(destEdit, 1);
    destRow->addWidget(browseBtn);
    lay->addLayout(destRow);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Exportar"));
    lay->addWidget(buttons);

    const auto chooseDest = [&]() {
        if (mp4Radio->isChecked()) {
            const QString f = QFileDialog::getSaveFileName(
                &dlg, tr("Vídeo de salida"), lastDir_, tr("MP4 (*.mp4)"));
            if (!f.isEmpty())
                destEdit->setText(f);
        } else {
            const QString dir = QFileDialog::getExistingDirectory(
                &dlg, tr("Carpeta de salida"), lastDir_);
            if (!dir.isEmpty())
                destEdit->setText(dir);
        }
    };
    connect(browseBtn, &QPushButton::clicked, &dlg, chooseDest);
    const auto updateFpsEnabled = [&]() {
        const bool isMp4 = mp4Radio->isChecked();
        fpsCombo->setEnabled(isMp4);
        smoothGroup->setEnabled(isMp4);
    };
    connect(mp4Radio, &QRadioButton::toggled, &dlg, updateFpsEnabled);
    updateFpsEnabled();
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;
    if (destEdit->text().isEmpty()) {
        QMessageBox::warning(this, tr("AstroTracker"),
                             tr("Elige un destino de exportación."));
        return;
    }

    PhotoExportWorker::Settings st;
    if (mp4Radio->isChecked()) {
        st.format = PhotoExportWorker::Format::Mp4;
        st.outFile = destEdit->text();
        bool okFps = false;
        const double fps = fpsCombo->currentText().toDouble(&okFps);
        st.fps = (okFps && fps > 0.0) ? fps : 10.0;
    } else {
        st.format = pngRadio->isChecked() ? PhotoExportWorker::Format::Png
                                          : PhotoExportWorker::Format::Jpg;
        st.outDir = destEdit->text();
    }
    st.resolution = static_cast<PhotoExportWorker::Resolution>(resCombo->currentData().toInt());
    st.borderMode = borderCombo_->currentIndex();
    st.interp = interpCombo->currentData().toInt();
    st.normalizeBrightness = brightChk->isChecked();

    // Si se elige "Directo", se exporta sin centrar: se pasa la lista de tracks
    // vacía para que el worker no aplique ningún desplazamiento (regla: nunca
    // descartar frames, se conserva el frame tal cual).
    const std::vector<DiscTrack> exportTracks =
        centeredRadio->isChecked() ? tracks_ : std::vector<DiscTrack>{};

    QStringList paths;
    paths.reserve(static_cast<int>(reader_.count()));
    for (int64_t i = 0; i < reader_.count(); ++i)
        paths.push_back(QString::fromStdString(reader_.filePath(i)));

    // Selección de exportación: las casillas marcadas en el filmstrip.
    std::vector<bool> selection;
    selection.reserve(static_cast<size_t>(reader_.count()));
    for (int64_t i = 0; i < reader_.count(); ++i) {
        bool sel = true;
        if (i < filmstrip_->count()) {
            QListWidgetItem* it = filmstrip_->item(static_cast<int>(i));
            if (it)
                sel = it->checkState() == Qt::Checked;
        }
        selection.push_back(sel);
    }

    const QString centringText = centeredRadio->isChecked() ? QStringLiteral("centrado")
                                                            : QStringLiteral("directo");
    AppLog::info(tr("Exportando %1 fotos seleccionadas de %2 (%3, %4, %5 fps, %6)")
                     .arg(static_cast<int>(std::count(selection.begin(), selection.end(), true)))
                     .arg(reader_.count())
                     .arg(st.format == PhotoExportWorker::Format::Mp4 ? QStringLiteral("MP4")
                                                                      : QStringLiteral("imágenes"))
                     .arg([&] {
                         switch (st.resolution) {
                         case PhotoExportWorker::Resolution::Original: return QStringLiteral("original");
                         case PhotoExportWorker::Resolution::Visor: return QStringLiteral("visor");
                         case PhotoExportWorker::Resolution::HD: return QStringLiteral("HD");
                         case PhotoExportWorker::Resolution::FHD: return QStringLiteral("FHD");
                         case PhotoExportWorker::Resolution::QHD: return QStringLiteral("2K");
                         case PhotoExportWorker::Resolution::UHD: return QStringLiteral("4K");
                         }
                         return QString();
                     }())
                     .arg(st.format == PhotoExportWorker::Format::Mp4
                              ? QStringLiteral("%1 fps, %2 intermedios%3")
                                    .arg(QString::number(st.fps))
                                    .arg(st.interp)
                                    .arg(st.normalizeBrightness ? QStringLiteral(", brillo")
                                                               : QString())
                              : QStringLiteral("-"))
                     .arg(centringText));

    exportWorker_ = new PhotoExportWorker(paths, exportTracks, displayMaxDim_, st, selection, this);
    connect(exportWorker_, &PhotoExportWorker::progress, this, &PhotoPanel::onWorkerProgress);
    connect(exportWorker_, &PhotoExportWorker::finished, this, &PhotoPanel::onExportFinished);
    connect(exportWorker_, &PhotoExportWorker::photoProcessed, this,
            &PhotoPanel::onPhotoProcessed);
    connect(exportWorker_, &QThread::finished, exportWorker_, &QObject::deleteLater);

    setTrackingBusy(true);
    emit statusMessage(tr("Exportando secuencia..."));
    emit workProgress(0, static_cast<int>(reader_.count()));
    exportWorker_->start();
}

void PhotoPanel::onExportFinished(bool ok, const QString& error, int frames)
{
    exportWorker_ = nullptr;
    setTrackingBusy(false);
    emit workProgress(-1, -1);
    if (ok) {
        emit statusMessage(tr("Exportación completada: %1 fotos").arg(frames), 6000);
        AppLog::info(tr("Exportación completada: %1 fotos").arg(frames));
    } else {
        emit statusMessage(tr("Exportación: %1")
                               .arg(error.isEmpty() ? tr("no se completó") : error), 0);
        AppLog::error(tr("Exportación: %1")
                          .arg(error.isEmpty() ? tr("no se completó") : error));
    }
}

void PhotoPanel::setTrackingBusy(bool busy)
{
    trackingBusy_ = busy;
    if (busy && playAction_ && playAction_->isChecked())
        playAction_->setChecked(false); // no reproducir mientras se calcula
    applyViewModes();
    stopAction_->setEnabled(busy);
    prevAction_->setEnabled(!busy);
    nextAction_->setEnabled(!busy);
    slider_->setEnabled(!busy && reader_.count() > 1);
    circleModeAction_->setEnabled(!busy);
    borderCombo_->setEnabled(!busy);
    updateTrackingUi();
}

void PhotoPanel::updateTrackingUi()
{
    const bool ready = reader_.isOpen() && !trackingBusy_;
    analyzeAction_->setEnabled(ready);
    fitAction_->setEnabled(ready);
    exportVideoAction_->setEnabled(ready);
    exportPhotosAction_->setEnabled(ready);
    lockAction_->setEnabled(ready && analyzed_);
    resetAction_->setEnabled(ready);
}

bool PhotoPanel::isBusy() const
{
    return worker_ != nullptr || exportWorker_ != nullptr;
}

double PhotoPanel::playbackFps() const
{
    if (fpsCombo_)
        return fpsCombo_->currentData().toDouble();
    return 5.0;
}

void PhotoPanel::setTrackingProfile(ObjectProfile profile)
{
    if (trackingProfile_.profile == profile)
        return;
    trackingProfile_ = trackingProfileFor(profile);
    QSettings settings;
    settings.setValue("Photos/profile", QLatin1String(objectProfileKey(profile)));
    AppLog::info(tr("Perfil de seguimiento: %1").arg(objectProfileName(profile)));
    emit modified();
    emit profileChanged(profile);
}

DiscMethod PhotoPanel::overrideFor(int64_t index) const
{
    const auto it = methodOverrides_.find(index);
    return it != methodOverrides_.end() ? it->second : DiscMethod::Prediction;
}

void PhotoPanel::setOverrideForCurrent(DiscMethod method)
{
    if (!reader_.isOpen())
        return;
    const int64_t idx = current_;
    if (method == DiscMethod::Prediction)
        methodOverrides_.erase(idx);
    else
        methodOverrides_[idx] = method;
    updateNavUi(); // refresca el texto de estado (y emite photoStatusChanged)
}

QString PhotoPanel::photoStatusText() const
{
    if (!reader_.isOpen())
        return QString();
    QString s;
    if (analyzed_ && current_ < static_cast<int64_t>(tracks_.size())) {
        const DiscTrack& t = tracks_[static_cast<size_t>(current_)];
        QString estado = t.predicted ? tr("supuesta")
                                     : (t.status == TrackStatus::VALID
                                            ? tr("válida")
                                            : tr("dudosa"));
        s = QStringLiteral("%1 · %2% · %3")
                .arg(QString::fromUtf8(methodName(t.method)))
                .arg(std::lround(t.confidence * 100.0f))
                .arg(estado);
    } else if (hasSeedCircle_) {
        s = tr("semilla sin calcular");
    } else {
        s = tr("sin análisis");
    }
    const auto it = methodOverrides_.find(current_);
    if (it != methodOverrides_.end())
        s += tr(" · fijada a %1").arg(QString::fromUtf8(methodName(it->second)));
    return s;
}

PhotoProjectPhotos PhotoPanel::collectState() const
{
    PhotoProjectPhotos d;
    if (!reader_.isOpen())
        return d;

    d.active = true;
    if (!sourceFolder_.isEmpty()) {
        d.originType = 0;
        d.originFolder = sourceFolder_;
    } else {
        d.originType = 1;
        d.originFiles = sourceFiles_;
    }
    d.analysisMaxDim = displayMaxDim_;
    d.currentIndex = static_cast<int>(current_);
    d.profile = trackingProfile_.profile;
    for (const auto& [idx, m] : methodOverrides_)
        d.overrides.push_back(PhotoProjectOverride{static_cast<int>(idx), m});
    d.hasSeed = hasSeedCircle_;
    d.seedIndex = static_cast<int>(seedIndex_);
    d.seedX = seedCircle_.center.x;
    d.seedY = seedCircle_.center.y;
    d.seedRadius = seedCircle_.radius;

    // Resultado disperso: solo se guardan fotos con círculo o con alguna marca
    // (bloqueada, fijada o excluida de la exportación).
    for (int64_t i = 0; i < reader_.count(); ++i) {
        const size_t idx = static_cast<size_t>(i);
        const bool hasCircle = i < static_cast<int64_t>(tracks_.size()) &&
                               tracks_[idx].radius > 0.f;
        const bool locked = i < static_cast<int64_t>(locked_.size()) && locked_[idx];
        const bool manual = i < static_cast<int64_t>(manualFixed_.size()) &&
                            manualFixed_[idx];
        const bool exportIt =
            i < static_cast<int64_t>(exportSelection_.size()) ? exportSelection_[idx] : true;
        if (!hasCircle && !locked && !manual && exportIt)
            continue;

        PhotoProjectPhotoResult r;
        r.file = QString::fromStdString(reader_.fileName(i));
        if (hasCircle) {
            r.x = tracks_[idx].center.x;
            r.y = tracks_[idx].center.y;
            r.radius = tracks_[idx].radius;
            r.status = static_cast<int>(tracks_[idx].status);
            r.predicted = tracks_[idx].predicted;
            r.confidence = tracks_[idx].confidence;
            r.method = tracks_[idx].method;
        }
        r.locked = locked;
        r.manualFixed = manual;
        r.exportSelected = exportIt;
        d.results.push_back(r);
    }
    return d;
}

bool PhotoPanel::applyState(const PhotoProjectPhotos& data, QString* error)
{
    auto fail = [error](const QString& msg) {
        if (error)
            *error = msg;
        return false;
    };
    if (worker_ || exportWorker_)
        return fail(tr("Hay un cálculo o una exportación en curso"));
    if (!data.active)
        return fail(tr("El proyecto no contiene secuencia de fotos"));

    // Origen: carpeta o lista de archivos. Si la carpeta guardada ya no tiene
    // imágenes soportadas se informa y no se cambia la sesión actual.
    if (data.originType == 1) {
        std::vector<std::string> v;
        v.reserve(static_cast<size_t>(data.originFiles.size()));
        for (const QString& p : data.originFiles)
            v.push_back(p.toStdString());
        if (!reader_.open(v))
            return fail(tr("Ninguno de los archivos del proyecto es una imagen "
                           "soportada (o ya no existen)"));
    } else {
        if (!reader_.openFolder(data.originFolder.toStdString()))
            return fail(tr("La carpeta del proyecto ya no contiene imágenes soportadas:\n%1")
                            .arg(data.originFolder));
    }

    // El espacio de coordenadas de los círculos depende de maxDim: restaurarlo
    // antes de recargar la secuencia.
    displayMaxDim_ = data.analysisMaxDim > 0 ? data.analysisMaxDim : 1600;
    reloadSequence();

    // Perfil y overrides por foto.
    trackingProfile_ = trackingProfileFor(data.profile);
    methodOverrides_.clear();
    for (const PhotoProjectOverride& ov : data.overrides)
        if (ov.index >= 0)
            methodOverrides_[static_cast<int64_t>(ov.index)] = ov.method;
    emit profileChanged(trackingProfile_.profile);

    // Restaurar resultados y marcas casilla a casilla por nombre de archivo
    // (tolera reordenar o añadir fotos a la carpeta).
    const int64_t total = reader_.count();
    tracks_.assign(static_cast<size_t>(total), DiscTrack{});
    locked_.assign(static_cast<size_t>(total), false);
    manualFixed_.assign(static_cast<size_t>(total), false);

    // Mapa foto -> item del filmstrip (por UserRole: puede haber menos items
    // que fotos si alguna miniatura no se pudo generar).
    std::vector<QListWidgetItem*> items(static_cast<size_t>(total), nullptr);
    for (int k = 0; k < filmstrip_->count(); ++k) {
        QListWidgetItem* it = filmstrip_->item(k);
        const int64_t id = it->data(Qt::UserRole).toLongLong();
        if (id >= 0 && id < total)
            items[static_cast<size_t>(id)] = it;
    }

    int restored = 0;
    for (int64_t i = 0; i < total; ++i) {
        const size_t idx = static_cast<size_t>(i);
        const QString name = QString::fromStdString(reader_.fileName(i));
        const int r = data.indexOfResult(name);
        if (r < 0)
            continue;
        const PhotoProjectPhotoResult& e =
            data.results[static_cast<size_t>(r)];
        DiscTrack& t = tracks_[idx];
        t.center = cv::Point2f(e.x, e.y);
        t.radius = e.radius;
        t.status = static_cast<TrackStatus>(
            e.status >= 0 && e.status <= 2 ? e.status : 2);
        t.predicted = e.predicted;
        t.confidence = e.confidence;
        t.method = e.method;
        locked_[idx] = e.locked;
        manualFixed_[idx] = e.manualFixed;
        if (e.radius > 0.f)
            ++restored;

        if (items[idx]) {
            items[idx]->setCheckState(e.exportSelected ? Qt::Checked : Qt::Unchecked);
            exportSelection_[idx] = e.exportSelected;
        }
    }
    analyzed_ = restored > 0;

    // Fotos del proyecto que ya no están en la secuencia actual.
    auto inReader = [this, total](const QString& name) {
        for (int64_t i = 0; i < total; ++i) {
            if (QString::fromStdString(reader_.fileName(i))
                    .compare(name, Qt::CaseInsensitive) == 0)
                return true;
        }
        return false;
    };
    int absent = 0;
    for (const PhotoProjectPhotoResult& e : data.results) {
        if (!inReader(e.file))
            ++absent;
    }

    if (data.hasSeed && data.seedIndex >= 0 && data.seedIndex < total) {
        seedCircle_ = CircleF{cv::Point2f(data.seedX, data.seedY), data.seedRadius};
        seedIndex_ = data.seedIndex;
        hasSeedCircle_ = true;
    }

    current_ = std::min<int64_t>(std::max<int64_t>(data.currentIndex, 0),
                                 std::max<int64_t>(total - 1, 0));

    updateFilmstripBadges();
    updateTrackingUi();
    showCurrent();
    updateNavUi();

    if (absent > 0)
        AppLog::warn(tr("%1 fotos del proyecto no están en la secuencia actual").arg(absent));
    emit statusMessage(tr("Trabajo restaurado: %1 de %2 fotos con resultado")
                           .arg(restored)
                           .arg(total), 6000);
    AppLog::info(tr("Trabajo restaurado: %1 de %2 fotos con resultado%3")
                     .arg(restored)
                     .arg(total)
                     .arg(absent > 0 ? tr(", %1 ausentes").arg(absent) : QString()));
    return true;
}

void PhotoPanel::runTracking()
{
    if (worker_ || !reader_.isOpen())
        return;

    if (!hasSeedCircle_) {
        AppLog::info(tr("Auto-detectando disco en la foto %1...").arg(current_ + 1));
        if (!autoDetectSeed()) {
            QMessageBox::warning(this, tr("AstroTracker"),
                tr("No se detectó disco en la foto %1.\n"
                   "Dibuja un círculo manualmente o prueba en otra foto.")
                    .arg(current_ + 1));
            return;
        }
        const auto btn = QMessageBox::question(this, tr("AstroTracker"),
            tr("Disco detectado en la foto %1 (centro=%2,%3 r=%4).\n"
               "¿Calcular automáticamente desde aquí?")
                .arg(current_ + 1)
                .arg(seedCircle_.center.x, 0, 'f', 0)
                .arg(seedCircle_.center.y, 0, 'f', 0)
                .arg(seedCircle_.radius, 0, 'f', 0));
        if (btn != QMessageBox::Yes)
            return;
    }

    AppLog::info(tr("Cálculo automático desde el círculo de la foto %1 (%2,%3 r%4)")
                     .arg(seedIndex_ + 1)
                     .arg(seedCircle_.center.x, 0, 'f', 0)
                     .arg(seedCircle_.center.y, 0, 'f', 0)
                     .arg(seedCircle_.radius, 0, 'f', 0));
    AppLog::info(tr("Perfil: %1 | Métodos: %2")
                     .arg(objectProfileName(trackingProfile_.profile))
                     .arg(profileMethodList(trackingProfile_.profile)));
    if (!methodOverrides_.empty())
        AppLog::info(tr("Overrides por foto: %1 fotos con método fijado")
                         .arg(methodOverrides_.size()));

    QStringList paths;
    paths.reserve(static_cast<int>(reader_.count()));
    for (int64_t i = 0; i < reader_.count(); ++i)
        paths.push_back(QString::fromStdString(reader_.filePath(i)));

    // Máscara de fotos "protegidas": bloqueadas + corregidas a mano. El worker
    // procesa la cadena para mantener la continuidad pero no sobrescribe su
    // resultado.
    std::vector<bool> mask = locked_;
    if (mask.size() < manualFixed_.size())
        mask.resize(manualFixed_.size(), false);
    for (size_t i = 0; i < manualFixed_.size(); ++i)
        if (manualFixed_[i])
            mask[i] = true;

    worker_ = new PhotoTrackWorker(paths, seedCircle_, seedIndex_, displayMaxDim_,
                                   DiscTrackerParams(), trackingProfile_,
                                   methodOverrides_, mask, this);

    connect(worker_, &PhotoTrackWorker::progress, this, &PhotoPanel::onWorkerProgress);
    connect(worker_, &PhotoTrackWorker::reacquired, this, &PhotoPanel::onWorkerReacquired);
    connect(worker_, &PhotoTrackWorker::finished, this, &PhotoPanel::onWorkerFinished);
    connect(worker_, &PhotoTrackWorker::photoProcessed, this, &PhotoPanel::onPhotoProcessed);
    connect(worker_, &QThread::finished, worker_, &QObject::deleteLater);

    setTrackingBusy(true);
    emit statusMessage(tr("Cálculo automático del centrado a partir del círculo de "
                          "la foto %1...")
                           .arg(seedIndex_ + 1));
    emit workProgress(0, static_cast<int>(reader_.count()));
    worker_->start();
}

void PhotoPanel::stopTracking()
{
    if (exportWorker_)
        exportWorker_->requestStop();
    if (worker_)
        worker_->requestStop();
}

void PhotoPanel::onWorkerProgress(int done, int total)
{
    emit workProgress(done, total);
}

void PhotoPanel::onPhotoProcessed(int64_t index)
{
    if (index < 0 || index >= reader_.count())
        return;
    emit statusMessage(tr("Procesando %1 (%2/%3)…")
                           .arg(QString::fromStdString(reader_.fileName(index)))
                           .arg(index + 1)
                           .arg(reader_.count()));

    // Mostrar en vivo la foto que se está procesando, sin tocar current_ (la
    // navegación y el loader siguen en su sitio; al terminar se restaura la
    // foto actual). Izquierdo: original. Derecho: centrada si hay resultado;
    // si no, directa (va mostrando el avance del cálculo).
    cv::Mat frame;
    if (reader_.readAt(index, frame, displayMaxDim_) && !frame.empty()) {
        view_->setFrame(frame);
        if (analyzed_ && index < static_cast<int64_t>(tracks_.size())) {
            const DiscTrack& t = tracks_[static_cast<size_t>(index)];
            if (t.radius > 0.f) {
                resultView_->setFrame(centeredFrame(frame, t));
                resultView_->setCircle(QPointF(frame.cols / 2.0, frame.rows / 2.0),
                                       t.radius, t.predicted);
                return;
            }
        }
        resultView_->setFrame(frame);
        resultView_->clearCircle();
    }
}

void PhotoPanel::onFrameReady(int64_t index, const cv::Mat& frame)
{
    if (index != current_ || frame.empty())
        return;
    loadingView_ = false;
    applyViewModes();
    view_->setLoading(false);
    resultView_->setLoading(false);
    view_->setFrame(frame);
    updateViewerCircles(frame);
    // El EXIF y la info del fichero también se actualizan al cargar por el
    // loader (la ruta síncrona showCurrentSync se usa solo sin loader).
    emitFileInfo(index);
}

void PhotoPanel::onFilmstripChanged(QListWidgetItem* item)
{
    // La selección de exportación vive en las casillas del filmstrip; aquí solo
    // se detecta el cambio del usuario (los refrescos de etiquetas no tocan el
    // estado de la casilla) para marcar el proyecto como modificado.
    if (!item)
        return;
    const int64_t idx = item->data(Qt::UserRole).toLongLong();
    if (idx < 0 || idx >= static_cast<int64_t>(exportSelection_.size()))
        return;
    const bool checked = item->checkState() == Qt::Checked;
    if (checked == exportSelection_[static_cast<size_t>(idx)])
        return;
    exportSelection_[static_cast<size_t>(idx)] = checked;
    AppLog::info(checked ? tr("Foto %1 marcada para exportar").arg(idx + 1)
                         : tr("Foto %1 excluida de la exportación").arg(idx + 1));
    emit modified();
}

void PhotoPanel::onWorkerReacquired(int64_t index, int predictedBefore)
{
    emit statusMessage(tr("Re-encontrado el disco en la foto %1 tras %2 fotos "
                          "supuestas. Ampliando la búsqueda...")
                           .arg(index + 1)
                           .arg(predictedBefore), 3000);
}

void PhotoPanel::onWorkerFinished(bool ok, const QString&, const QVector<double>& results)
{
    if (ok && !results.isEmpty()) {
        const int stride = 7;
        const int total = static_cast<int>(results.size() / stride);
        const std::vector<DiscTrack> old = std::move(tracks_);
        tracks_.clear();
        tracks_.reserve(static_cast<size_t>(total));
        for (int i = 0; i < total; ++i) {
            const int at = i * stride;
            // Fotos protegidas (bloqueadas o corregidas a mano): conservan su
            // valor anterior (el worker no escribe su resultado).
            const bool keep =
                (static_cast<size_t>(i) < locked_.size() &&
                 locked_[static_cast<size_t>(i)]) ||
                (static_cast<size_t>(i) < manualFixed_.size() &&
                 manualFixed_[static_cast<size_t>(i)]);
            if (keep && static_cast<size_t>(i) < old.size() &&
                old[static_cast<size_t>(i)].radius > 0.f) {
                tracks_.push_back(old[static_cast<size_t>(i)]);
                continue;
            }
            DiscTrack t;
            t.center = cv::Point2f(static_cast<float>(results[at]),
                                   static_cast<float>(results[at + 1]));
            t.radius = static_cast<float>(results[at + 2]);
            t.status = static_cast<TrackStatus>(static_cast<int>(results[at + 3]));
            t.predicted = results[at + 4] > 0.5;
            t.confidence = static_cast<float>(results[at + 5]);
            const int m = static_cast<int>(results[at + 6]);
            t.method = (m >= 0 && m <= static_cast<int>(DiscMethod::Centroid))
                           ? static_cast<DiscMethod>(m)
                           : DiscMethod::Prediction;
            tracks_.push_back(t);
        }
        analyzed_ = !tracks_.empty();
    }
    worker_ = nullptr;

    setTrackingBusy(false);
    updateNavUi();
    showCurrent();
    updateTrackingUi();
    updateFilmstripBadges();
    emit workProgress(-1, -1);

    if (!ok || results.isEmpty()) {
        tracks_.clear();
        analyzed_ = false;
        emit statusMessage(tr("El seguimiento no pudo confirmar el disco"), 0);
        return;
    }

    if (analyzed_) {
        int valid = 0, predicted = 0, fixed = 0;
        for (size_t i = 0; i < tracks_.size(); ++i) {
            const DiscTrack& t = tracks_[i];
            const bool isFixed = (i < locked_.size() && locked_[i]) ||
                                 (i < manualFixed_.size() && manualFixed_[i]);
            if (isFixed)
                ++fixed;
            else if (t.predicted)
                ++predicted;
            else if (t.status == TrackStatus::VALID)
                ++valid;
        }
        emit statusMessage(tr("Cálculo: %1 fotos, %2 válidas, %3 supuestas, %4 fijadas")
                               .arg(tracks_.size())
                               .arg(valid)
                               .arg(predicted)
                               .arg(fixed));
        AppLog::info(tr("Cálculo terminado: %1 fotos, %2 válidas, %3 supuestas, %4 fijadas")
                         .arg(tracks_.size())
                         .arg(valid)
                         .arg(predicted)
                         .arg(fixed));
        emit modified();
    } else {
        emit statusMessage(tr("El seguimiento no pudo confirmar el disco"), 0);
        AppLog::error(tr("Seguimiento sin confirmación del disco"));
    }
}

cv::Mat PhotoPanel::centeredFrame(const cv::Mat& frame, const DiscTrack& track)
{
    const cv::Point2f offset(frame.cols / 2.0f - track.center.x,
                             frame.rows / 2.0f - track.center.y);
    const auto mode = (borderCombo_->currentIndex() == 1) ? BorderMode::Replicate
                                                          : BorderMode::Black;
    return BorderHandler::apply(frame, offset, mode);
}

cv::Mat PhotoPanel::centeredFrame(const cv::Mat& frame, const CircleF& circle)
{
    const cv::Point2f offset(frame.cols / 2.0f - circle.center.x,
                             frame.rows / 2.0f - circle.center.y);
    const auto mode = (borderCombo_->currentIndex() == 1) ? BorderMode::Replicate
                                                          : BorderMode::Black;
    return BorderHandler::apply(frame, offset, mode);
}

QPixmap PhotoPanel::toPixmap(const cv::Mat& bgr)
{
    return matToPixmap(bgr);
}