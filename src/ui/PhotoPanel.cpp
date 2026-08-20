#include "ui/PhotoPanel.h"

#include "processing/BorderHandler.h"
#include "stills/PhotoExportWorker.h"
#include "stills/PhotoTrackWorker.h"
#include "tracking/DiscArcFit.h"
#include "ui/VideoView.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
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

    tb->addSeparator();
    prevAction_ = tb->addAction(tr("Anterior"), this, &PhotoPanel::showPrev);
    nextAction_ = tb->addAction(tr("Siguiente"), this, &PhotoPanel::showNext);
    tb->addSeparator();
    analyzeAction_ = tb->addAction(tr("Calcular automáticamente"));
    analyzeAction_->setEnabled(false);
    analyzeAction_->setToolTip(tr("Seguir el disco en todas las fotos desde el círculo "
                                  "de la semilla. Las fotos bloqueadas no se modifican."));
    connect(analyzeAction_, &QAction::triggered, this, &PhotoPanel::runTracking);
    fitAction_ = tb->addAction(tr("Ajustar fotograma"));
    fitAction_->setEnabled(false);
    fitAction_->setToolTip(tr("Detectar el disco solo en la foto actual: ajusta el "
                              "círculo que forma la fase visible (parcial, creciente o "
                              "corona)"));
    connect(fitAction_, &QAction::triggered, this, &PhotoPanel::onFitDisc);
    exportAction_ = tb->addAction(tr("Exportar centradas..."));
    exportAction_->setEnabled(false);
    exportAction_->setToolTip(tr("Guardar las fotos centradas como imágenes o como vídeo MP4"));
    connect(exportAction_, &QAction::triggered, this, &PhotoPanel::startExport);
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

    filmstrip_ = new QListWidget(this);
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
    connect(slider_, &QSlider::valueChanged, this, &PhotoPanel::onSliderChanged);
    connect(view_, &VideoView::roiSelected, this, &PhotoPanel::onRoiSelected);
    connect(view_, &VideoView::circleSelected, this, &PhotoPanel::onCircleSelected);

    prevAction_->setEnabled(false);
    nextAction_->setEnabled(false);
    applyViewModes();

    QSettings settings;
    recentFolders_ = settings.value("Photos/recentFolders").toStringList();
    lastDir_ = settings.value("Photos/lastDir").toString();
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
    rememberFolder(dir);
    reloadSequence();
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
    rememberFolder(QFileInfo(paths.first()).absolutePath());
    reloadSequence();
}

void PhotoPanel::clearSession()
{
    reader_.close();
    current_ = 0;
    filmstrip_->clear();
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
    buildFilmstrip();

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
    emit workProgress(0, static_cast<int>(total));
    emit statusMessage(tr("Generando miniaturas 0/%1...").arg(total));

    for (int64_t i = 0; i < total; ++i) {
        cv::Mat thumb;
        if (!reader_.thumbnail(i, thumb, thumbMaxDim_))
            continue;
        auto* item = new QListWidgetItem(QIcon(matToPixmap(thumb)), QString());
        item->setToolTip(QString::fromStdString(reader_.fileName(i)));
        item->setData(Qt::UserRole, static_cast<qlonglong>(i));
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

void PhotoPanel::showCurrent()
{
    cv::Mat frame;
    if (!reader_.readAt(current_, frame, displayMaxDim_)) {
        view_->setFrame(cv::Mat());
        resultView_->setFrame(cv::Mat());
        return;
    }
    view_->setFrame(frame);

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
        // Edición manual: solo se actualiza esta foto, sin relanzar nada.
        DiscTrack& t = tracks_[static_cast<size_t>(current_)];
        t.center = center;
        t.radius = radius;
        t.status = TrackStatus::VALID;
        t.predicted = false;
        updateFilmstripBadges();
        showCurrent();
        updateNavUi();
        emit statusMessage(
            tr("Círculo ajustado en la foto %1. Usa \"Calcular automáticamente\" para "
               "revisar el resto, o \"Bloquear fotograma\" para fijarla.")
                .arg(current_ + 1));
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
}

void PhotoPanel::onLockToggle(bool locked)
{
    if (locked_.size() <= static_cast<size_t>(current_))
        locked_.resize(static_cast<size_t>(current_) + 1, false);
    locked_[static_cast<size_t>(current_)] = locked;
    updateFilmstripBadges();
    updateNavUi();
    emit statusMessage(locked ? tr("Foto %1 bloqueada: no se modificará en el cálculo "
                                   "automático.")
                                    .arg(current_ + 1)
                              : tr("Foto %1 desbloqueada.")
                                    .arg(current_ + 1));
}

void PhotoPanel::applyViewModes()
{
    view_->setCircleEnabled(drawCircleMode_ && !trackingBusy_);
    view_->setRoiEnabled(!drawCircleMode_ && !trackingBusy_);
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
        return;
    }
    applyCircle(e.center, e.radius);
}

void PhotoPanel::startExport()
{
    if (exportWorker_ || worker_ || !analyzed_ || tracks_.empty())
        return;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Exportar centradas"));
    auto* lay = new QVBoxLayout(&dlg);

    auto* fmtLabel = new QLabel(tr("Formato"), &dlg);
    lay->addWidget(fmtLabel);
    auto* jpgRadio = new QRadioButton(tr("Fotos JPG"), &dlg);
    auto* pngRadio = new QRadioButton(tr("Fotos PNG"), &dlg);
    auto* mp4Radio = new QRadioButton(tr("Vídeo MP4"), &dlg);
    jpgRadio->setChecked(true);
    jpgRadio->setToolTip(tr("Una imagen por foto, centrada"));
    pngRadio->setToolTip(tr("Una imagen por foto, centrada (sin pérdida)"));
    mp4Radio->setToolTip(tr("Un vídeo H.264 (MP4) con todas las fotos centradas"));
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
    const auto updateFpsEnabled = [&]() { fpsCombo->setEnabled(mp4Radio->isChecked()); };
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

    QStringList paths;
    paths.reserve(static_cast<int>(reader_.count()));
    for (int64_t i = 0; i < reader_.count(); ++i)
        paths.push_back(QString::fromStdString(reader_.filePath(i)));

    exportWorker_ = new PhotoExportWorker(paths, tracks_, displayMaxDim_, st, this);
    connect(exportWorker_, &PhotoExportWorker::progress, this, &PhotoPanel::onWorkerProgress);
    connect(exportWorker_, &PhotoExportWorker::finished, this, &PhotoPanel::onExportFinished);
    connect(exportWorker_, &QThread::finished, exportWorker_, &QObject::deleteLater);

    setTrackingBusy(true);
    emit statusMessage(tr("Exportando centradas..."));
    emit workProgress(0, static_cast<int>(reader_.count()));
    exportWorker_->start();
}

void PhotoPanel::onExportFinished(bool ok, const QString& error, int frames)
{
    exportWorker_ = nullptr;
    setTrackingBusy(false);
    emit workProgress(-1, -1);
    if (ok)
        emit statusMessage(tr("Exportación completada: %1 fotos").arg(frames), 6000);
    else
        emit statusMessage(tr("Exportación: %1")
                               .arg(error.isEmpty() ? tr("no se completó") : error), 0);
}

void PhotoPanel::setTrackingBusy(bool busy)
{
    trackingBusy_ = busy;
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
    analyzeAction_->setEnabled(ready && hasSeedCircle_);
    fitAction_->setEnabled(ready);
    exportAction_->setEnabled(ready && analyzed_ && !tracks_.empty());
    lockAction_->setEnabled(ready && analyzed_);
}

void PhotoPanel::runTracking()
{
    if (worker_ || !reader_.isOpen() || !hasSeedCircle_)
        return;

    QStringList paths;
    paths.reserve(static_cast<int>(reader_.count()));
    for (int64_t i = 0; i < reader_.count(); ++i)
        paths.push_back(QString::fromStdString(reader_.filePath(i)));

    worker_ = new PhotoTrackWorker(paths, seedCircle_, seedIndex_, displayMaxDim_,
                                   DiscTrackerParams(), locked_, this);

    connect(worker_, &PhotoTrackWorker::progress, this, &PhotoPanel::onWorkerProgress);
    connect(worker_, &PhotoTrackWorker::reacquired, this, &PhotoPanel::onWorkerReacquired);
    connect(worker_, &PhotoTrackWorker::finished, this, &PhotoPanel::onWorkerFinished);
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
    emit statusMessage(tr("Procesando foto %1/%2...").arg(done).arg(total));
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
        const int stride = 5;
        const int total = static_cast<int>(results.size() / stride);
        const std::vector<DiscTrack> old = std::move(tracks_);
        tracks_.clear();
        tracks_.reserve(static_cast<size_t>(total));
        for (int i = 0; i < total; ++i) {
            const int at = i * stride;
            // Fotos bloqueadas: conservan su valor anterior (el worker no escribe
            // su resultado).
            if (static_cast<size_t>(i) < locked_.size() && locked_[static_cast<size_t>(i)] &&
                static_cast<size_t>(i) < old.size() && old[static_cast<size_t>(i)].radius > 0.f) {
                tracks_.push_back(old[static_cast<size_t>(i)]);
                continue;
            }
            DiscTrack t;
            t.center = cv::Point2f(static_cast<float>(results[at]),
                                   static_cast<float>(results[at + 1]));
            t.radius = static_cast<float>(results[at + 2]);
            t.status = static_cast<TrackStatus>(static_cast<int>(results[at + 3]));
            t.predicted = results[at + 4] > 0.5;
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
        int valid = 0, predicted = 0, blocked = 0;
        for (size_t i = 0; i < tracks_.size(); ++i) {
            const DiscTrack& t = tracks_[i];
            if (i < locked_.size() && locked_[i])
                ++blocked;
            else if (t.predicted)
                ++predicted;
            else if (t.status == TrackStatus::VALID)
                ++valid;
        }
        emit statusMessage(tr("Cálculo: %1 fotos, %2 válidas, %3 supuestas, %4 bloqueadas")
                               .arg(tracks_.size())
                               .arg(valid)
                               .arg(predicted)
                               .arg(blocked));
    } else {
        emit statusMessage(tr("El seguimiento no pudo confirmar el disco"), 0);
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