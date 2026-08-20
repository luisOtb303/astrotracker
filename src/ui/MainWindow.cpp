#include "ui/MainWindow.h"

#include "common/AppLog.h"
#include "ui/PhotoPanel.h"
#include "ui/VideoView.h"
#include "video/IVideoReader.h"
#include "video/FFmpegVideoReader.h"
#include "processing/FrameTransformer.h"
#include "export/PipelineWorker.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QDir>
#include <QFileInfo>
#include <QProgressBar>
#include <QSettings>
#include <QSlider>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QTime>
#include <algorithm>
#include <cmath>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();
    timer_ = new QTimer(this);
    timer_->setInterval(33);
    connect(timer_, &QTimer::timeout, this, &MainWindow::onTimer);
    connect(view_, &VideoView::roiSelected, this, &MainWindow::onRoiSelected);
    updateStabilizationUi();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    setWindowTitle(tr("AstroTracker"));
    resize(1100, 720);

    QMenu* fileMenu = menuBar()->addMenu(tr("&Archivo"));
    QAction* openAction = fileMenu->addAction(tr("&Abrir vídeo..."), this,
                                              &MainWindow::openFile, QKeySequence::Open);
    fileMenu->addAction(tr("Abrir &fotos (secuencia)..."), this, &MainWindow::openPhotos);
    fileMenu->addSeparator();
    QMenu* recentsMenu = fileMenu->addMenu(tr("&Recientes"));
    connect(recentsMenu, &QMenu::aboutToShow, this, [this, recentsMenu] {
        populateRecentsMenu(recentsMenu);
    });
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Salir"), this, &QWidget::close);

    QMenu* viewMenu = menuBar()->addMenu(tr("&Ver"));
    logDockAction_ = viewMenu->addAction(tr("&Salida"));
    logDockAction_->setCheckable(true);
    logDockAction_->setChecked(true);
    connect(logDockAction_, &QAction::toggled, this, [this](bool on) {
        if (logDock_)
            logDock_->setVisible(on);
    });

    QToolBar* tb = addToolBar(tr("Reproducción"));
    tb->setMovable(false);

    QAction* openTb = tb->addAction(style()->standardIcon(QStyle::SP_DialogOpenButton),
                                    tr("Abrir"), this, &MainWindow::openFile);
    playAction_ = tb->addAction(style()->standardIcon(QStyle::SP_MediaPlay),
                                tr("Reproducir/Pausar"), this, &MainWindow::playPause);
    playAction_->setEnabled(false);
    tb->addAction(style()->standardIcon(QStyle::SP_MediaSkipBackward),
                  tr("Atrás"), this, &MainWindow::stepBackward);
    tb->addAction(style()->standardIcon(QStyle::SP_MediaSkipForward),
                  tr("Adelante"), this, &MainWindow::stepForward);
    tb->addAction(style()->standardIcon(QStyle::SP_MediaStop),
                  tr("Detener"), this, &MainWindow::stop);

    QToolBar* stabTb = addToolBar(tr("Estabilización"));
    stabTb->setMovable(false);

    trackerCombo_ = new QComboBox(stabTb);
    trackerCombo_->addItem(tr("Template"));
    trackerCombo_->addItem(tr("Centroid"));
    trackerCombo_->setToolTip(tr("Algoritmo de seguimiento"));
    stabTb->addWidget(trackerCombo_);

    borderCombo_ = new QComboBox(stabTb);
    borderCombo_->addItem(tr("Borde negro"));
    borderCombo_->addItem(tr("Borde réplica"));
    borderCombo_->setToolTip(tr("Relleno de los bordes al desplazar el frame"));
    stabTb->addWidget(borderCombo_);

    smoothSpin_ = new QDoubleSpinBox(stabTb);
    smoothSpin_->setRange(0.01, 1.0);
    smoothSpin_->setSingleStep(0.05);
    smoothSpin_->setValue(0.3);
    smoothSpin_->setToolTip(tr("Suavizado (alpha EMA) del centro del objeto"));
    stabTb->addWidget(smoothSpin_);

    analyzeAction_ = stabTb->addAction(tr("Seguir"), this, &MainWindow::startAnalyze);
    analyzeAction_->setToolTip(tr("Analizar el vídeo: seguir el objeto y calcular desplazamientos"));
    previewAction_ = stabTb->addAction(tr("Vista previa"), this, &MainWindow::togglePreview);
    previewAction_->setCheckable(true);
    previewAction_->setToolTip(tr("Mostrar los frames estabilizados"));
    exportAction_ = stabTb->addAction(tr("Exportar..."), this, &MainWindow::startExport);
    exportAction_->setToolTip(tr("Estabilizar y guardar el vídeo de salida"));

    connect(borderCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { showCurrentFrame(); });

    tabs_ = new QTabWidget(this);
    tabs_->addTab(createVideoPage(), tr("Vídeo"));
    photosPanel_ = new PhotoPanel(this);
    tabs_->addTab(photosPanel_, tr("Fotos"));
    setCentralWidget(tabs_);

    connect(photosPanel_, &PhotoPanel::statusMessage, this,
            [this](const QString& msg, int timeoutMs) {
                statusBar()->showMessage(msg, timeoutMs);
            });
    connect(photosPanel_, &PhotoPanel::workProgress, this, [this](int done, int total) {
        if (!progressBar_)
            return;
        if (total <= 0) {
            progressBar_->setVisible(false);
            return;
        }
        progressBar_->setRange(0, total);
        progressBar_->setValue(done);
        progressBar_->setVisible(true);
    });

    progressBar_ = new QProgressBar(this);
    progressBar_->setVisible(false);
    statusBar()->addPermanentWidget(progressBar_);
    statusBar()->showMessage(tr("Abrir un vídeo o una secuencia de fotos para empezar"));

    logDock_ = new QDockWidget(tr("Salida"), this);
    logDock_->setObjectName(QStringLiteral("logDock"));
    logDock_->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::RightDockWidgetArea);
    auto* logWidget = new QWidget(logDock_);
    auto* logLay = new QVBoxLayout(logWidget);
    logLay->setContentsMargins(4, 2, 4, 2);
    logLay->setSpacing(2);
    auto* logBar = new QHBoxLayout();
    debugCheck_ = new QCheckBox(tr("Depuración"), logWidget);
    debugCheck_->setToolTip(tr("Mostrar líneas de depuración en la salida"));
    auto* clearBtn = new QPushButton(tr("Vaciar"), logWidget);
    connect(clearBtn, &QPushButton::clicked, this, [this] { logView_->clear(); });
    logBar->addWidget(debugCheck_);
    logBar->addWidget(clearBtn);
    logBar->addStretch(1);
    logLay->addLayout(logBar);
    logView_ = new QPlainTextEdit(logWidget);
    logView_->setReadOnly(true);
    logView_->setMaximumBlockCount(2000);
    logView_->setLineWrapMode(QPlainTextEdit::NoWrap);
    logView_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    logLay->addWidget(logView_, 1);
    logDock_->setWidget(logWidget);
    addDockWidget(Qt::BottomDockWidgetArea, logDock_);
    resizeDocks({logDock_}, {160}, Qt::Vertical);

    connect(&AppLog::instance(), &AppLog::message, this, &MainWindow::onLogMessage);

    connect(slider_, &QSlider::valueChanged, this, [this](int ms) {
        if (!reader_ || ms == currentUs_ / 1000)
            return;
        reader_->seekToUs(static_cast<int64_t>(ms) * 1000);
        showCurrentFrame();
    });
}

QWidget* MainWindow::createVideoPage()
{
    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(4, 4, 4, 4);

    auto* viewers = new QHBoxLayout();

    auto* sourceBox = new QVBoxLayout();
    auto* srcTitle = new QLabel(tr("Original"), central);
    srcTitle->setAlignment(Qt::AlignCenter);
    sourceBox->addWidget(srcTitle);
    view_ = new VideoView(central);
    sourceBox->addWidget(view_, 1);

    auto* resultBox = new QVBoxLayout();
    auto* resTitle = new QLabel(tr("Estabilizado"), central);
    resTitle->setAlignment(Qt::AlignCenter);
    resultBox->addWidget(resTitle);
    resultView_ = new VideoView(central);
    resultView_->setRoiEnabled(false);
    resultBox->addWidget(resultView_, 1);

    viewers->addLayout(sourceBox, 1);
    viewers->addLayout(resultBox, 1);
    root->addLayout(viewers, 1);

    slider_ = new QSlider(Qt::Horizontal, central);
    slider_->setEnabled(false);
    root->addWidget(slider_);

    auto* bottom = new QHBoxLayout();
    frameLabel_ = new QLabel(tr("Frame: - / -"), central);
    timeLabel_ = new QLabel(tr("00:00:00.000"), central);
    timeLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    bottom->addWidget(frameLabel_, 1);
    bottom->addWidget(timeLabel_);
    root->addLayout(bottom);

    return central;
}

void MainWindow::openPhotos()
{
    tabs_->setCurrentWidget(photosPanel_);
    photosPanel_->openImagesDialog();
}

void MainWindow::rememberVideoPath(const QString& path)
{
    if (path.isEmpty())
        return;
    QSettings settings;
    QStringList recent = settings.value("Video/recentFiles").toStringList();
    recent.removeAll(path);
    recent.push_front(path);
    while (recent.size() > 8)
        recent.removeLast();
    settings.setValue("Video/recentFiles", recent);
    settings.setValue("Video/lastDir", QFileInfo(path).absolutePath());
}

void MainWindow::populateRecentsMenu(QMenu* menu)
{
    menu->clear();

    QSettings settings;
    const QStringList videoFiles = settings.value("Video/recentFiles").toStringList();
    const QStringList photoDirs = photosPanel_->recentFolders();

    if (videoFiles.isEmpty() && photoDirs.isEmpty()) {
        QAction* none = menu->addAction(tr("(sin recientes)"));
        none->setEnabled(false);
        return;
    }

    if (!videoFiles.isEmpty()) {
        QAction* title = menu->addAction(tr("Vídeos"));
        title->setEnabled(false);
        for (const QString& v : videoFiles) {
            QAction* a = menu->addAction(QFileInfo(v).fileName());
            a->setToolTip(v);
            connect(a, &QAction::triggered, this,
                    [this, v] { openPath(v); });
        }
    }

    if (!photoDirs.isEmpty()) {
        menu->addSeparator();
        QAction* title = menu->addAction(tr("Carpetas de fotos"));
        title->setEnabled(false);
        for (const QString& d : photoDirs) {
            QAction* a = menu->addAction(d);
            a->setToolTip(d);
            connect(a, &QAction::triggered, this, [this, d] {
                tabs_->setCurrentWidget(photosPanel_);
                photosPanel_->openRecentFolder(d);
            });
        }
    }
}

void MainWindow::openFile()
{
    QSettings settings;
    QString startDir;
    if (settings.contains("Video/lastDir")) {
        const QString d = settings.value("Video/lastDir").toString();
        if (QDir(d).exists())
            startDir = d;
    }

    const QString path = QFileDialog::getOpenFileName(
        this, tr("Abrir vídeo"), startDir,
        tr("Vídeo (*.mp4 *.mov *.avi *.mkv *.ser *.mts *.m2ts);;Todos los archivos (*.*)"));

    if (path.isEmpty())
        return;

    openPath(path);
}

void MainWindow::openPath(const QString& path)
{
    auto reader = std::make_unique<FFmpegVideoReader>();
    if (!reader->open(path.toStdString())) {
        QMessageBox::critical(this, tr("AstroTracker"),
                              tr("No se pudo abrir el vídeo:\n%1").arg(path));
        return;
    }

    rememberVideoPath(path);

    inPath_ = path;
    reader_ = std::move(reader);
    totalUs_ = reader_->durationUs();
    totalFrames_ = reader_->frameCount();
    if (reader_->fps() > 0.0)
        stepUs_ = static_cast<int64_t>(1000000.0 / reader_->fps());
    currentUs_ = 0;
    roi_ = QRect();
    offsets_.clear();
    previewEnabled_ = false;
    previewAction_->setChecked(false);
    view_->setRoiEnabled(true);
    view_->clearRoi();
    startUs_ = 0;
    startIndex_ = 0;

    slider_->setRange(0, static_cast<int>(totalUs_ / 1000));
    slider_->setValue(0);
    slider_->setEnabled(true);
    playAction_->setEnabled(true);

    showCurrentFrame();
    updateStabilizationUi();
    statusBar()->showMessage(
        tr("Abierto: %1  (%2x%3, %4 fps, %5 frames)")
            .arg(path)
            .arg(reader_->width())
            .arg(reader_->height())
            .arg(reader_->fps(), 0, 'f', 2)
            .arg(totalFrames_));
}

void MainWindow::showCurrentFrame()
{
    Frame frame;
    if (!reader_ || !reader_->readNext(frame))
        return;
    currentUs_ = frame.ptsUs;
    view_->setFrame(frame.image);
    resultView_->setFrame(displayFrame(frame.image, frame.index));

    const int ms = static_cast<int>(currentUs_ / 1000);
    slider_->setValue(ms);

    frameLabel_->setText(tr("Frame: %1 / %2")
                             .arg(frame.index)
                             .arg(totalFrames_));
    timeLabel_->setText(formatTime(currentUs_));
}

void MainWindow::playPause()
{
    if (!reader_)
        return;
    if (timer_->isActive()) {
        timer_->stop();
        playAction_->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    } else {
        timer_->start();
        playAction_->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    }
}

void MainWindow::stop()
{
    timer_->stop();
    playAction_->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    if (reader_) {
        reader_->seekToUs(0);
        showCurrentFrame();
    }
}

void MainWindow::stepForward()
{
    if (!reader_)
        return;
    showCurrentFrame();
}

void MainWindow::stepBackward()
{
    if (!reader_)
        return;
    const int64_t target = std::max<int64_t>(0, currentUs_ - stepUs_);
    reader_->seekToUs(target);
    showCurrentFrame();
}

void MainWindow::onTimer()
{
    if (!reader_)
        return;
    Frame frame;
    if (!reader_->readNext(frame)) {
        timer_->stop();
        playAction_->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        return;
    }
    currentUs_ = frame.ptsUs;
    view_->setFrame(frame.image);
    resultView_->setFrame(displayFrame(frame.image, frame.index));
    slider_->setValue(static_cast<int>(currentUs_ / 1000));
    frameLabel_->setText(tr("Frame: %1 / %2").arg(frame.index).arg(totalFrames_));
    timeLabel_->setText(formatTime(currentUs_));
}

void MainWindow::onRoiSelected(const QRect& rect)
{
    roi_ = rect;
    view_->setRoi(rect);
    startUs_ = currentUs_;
    startIndex_ = reader_ ? static_cast<int64_t>(std::llround(currentUs_ * reader_->fps() / 1e6)) : 0;
    statusBar()->showMessage(tr("ROI seleccionada: %1x%2 en (%3,%4)")
                                 .arg(rect.width())
                                 .arg(rect.height())
                                 .arg(rect.x())
                                 .arg(rect.y()),
                             5000);
    updateStabilizationUi();
}

void MainWindow::startAnalyze()
{
    if (!reader_ || roi_.isEmpty() || worker_)
        return;

    PipelineWorker::Request req;
    req.mode = PipelineWorker::Mode::Analyze;
    req.inPath = inPath_;
    req.roi = cv::Rect2f(roi_.x(), roi_.y(), roi_.width(), roi_.height());
    req.settings = currentSettings();
    req.startUs = startUs_;
    launchWorker(req);
}

void MainWindow::togglePreview(bool enabled)
{
    if (enabled && offsets_.empty()) {
        previewAction_->setChecked(false);
        return;
    }
    previewEnabled_ = enabled;
    view_->setRoiEnabled(!enabled);
    if (enabled)
        statusBar()->showMessage(tr("Vista previa: se muestran los frames estabilizados"), 5000);
    showCurrentFrame();
}

void MainWindow::startExport()
{
    if (!reader_ || roi_.isEmpty() || offsets_.empty() || worker_)
        return;

    const QString outPath = QFileDialog::getSaveFileName(
        this, tr("Exportar vídeo estabilizado"), QString(),
        tr("Vídeo MP4 (*.mp4)"));
    if (outPath.isEmpty())
        return;

    PipelineWorker::Request req;
    req.mode = PipelineWorker::Mode::Export;
    req.inPath = inPath_;
    req.outPath = outPath;
    req.roi = cv::Rect2f(roi_.x(), roi_.y(), roi_.width(), roi_.height());
    req.settings = currentSettings();
    req.startUs = startUs_;
    launchWorker(req);
}

void MainWindow::launchWorker(const PipelineWorker::Request& req)
{
    if (worker_)
        return;

    worker_ = new PipelineWorker(req, this);
    connect(worker_, &PipelineWorker::progress, this, &MainWindow::onWorkerProgress);
    connect(worker_, &PipelineWorker::analyzeFinished, this, &MainWindow::onAnalyzeFinished);
    connect(worker_, &PipelineWorker::exportFinished, this, &MainWindow::onExportFinished);
    connect(worker_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(worker_, &QThread::finished, this, &MainWindow::onWorkerFinished);

    setBusy(true);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    progressBar_->setVisible(true);
    worker_->start();
}

void MainWindow::onWorkerProgress(int done, int total)
{
    if (total > 0)
        progressBar_->setRange(0, total);
    progressBar_->setValue(done);
}

void MainWindow::onAnalyzeFinished(bool ok, const QString& error, QVector<QPointF> offsets,
                                   int frames, int valid, double meanConfidence)
{
    Q_UNUSED(error);
    if (ok && !offsets.isEmpty()) {
        offsets_.clear();
        offsets_.reserve(static_cast<size_t>(offsets.size()));
        for (const QPointF& p : offsets)
            offsets_.push_back(cv::Point2f(static_cast<float>(p.x()),
                                           static_cast<float>(p.y())));
        previewEnabled_ = true;
        previewAction_->setChecked(true);
        view_->setRoiEnabled(false);
        showCurrentFrame();
        statusBar()->showMessage(
            tr("Seguimiento: %1 frames, %2 válidos, confianza %3%")
                .arg(frames)
                .arg(valid)
                .arg(meanConfidence * 100.0, 0, 'f', 1));
    } else {
        statusBar()->showMessage(tr("No se pudo analizar el vídeo"));
    }
    updateStabilizationUi();
}

void MainWindow::onExportFinished(bool ok, const QString& error, int frames, int valid)
{
    if (ok)
        statusBar()->showMessage(
            tr("Exportado: %1 (%2 frames, %3 válidos)").arg(inPath_).arg(frames).arg(valid));
    else
        statusBar()->showMessage(tr("Error al exportar: %1").arg(error));
}

void MainWindow::onWorkerFinished()
{
    worker_ = nullptr;
    setBusy(false);
    progressBar_->setVisible(false);
    updateStabilizationUi();
}

void MainWindow::onLogMessage(int level, const QString& text)
{
    if (level == AppLog::Debug && debugCheck_ && !debugCheck_->isChecked())
        return;
    QString color;
    QString tag;
    switch (level) {
    case AppLog::Error:
        color = QStringLiteral("#c00");
        tag = tr("[error]");
        break;
    case AppLog::Warn:
        color = QStringLiteral("#a06000");
        tag = tr("[aviso]");
        break;
    case AppLog::Debug:
        color = QStringLiteral("#888");
        tag = tr("[debug]");
        break;
    default:
        color = QStringLiteral("#222");
        break;
    }
    QString line = tag.isEmpty() ? text : tag + QStringLiteral(" ") + text;
    line.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    line.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    line.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    logView_->appendHtml(QStringLiteral("<span style=\"color:%1\">%2</span>")
                             .arg(color, line));
}

void MainWindow::updateStabilizationUi()
{
    const bool canTrack = reader_ != nullptr && !roi_.isEmpty() && worker_ == nullptr;
    analyzeAction_->setEnabled(canTrack);
    exportAction_->setEnabled(canTrack && !offsets_.empty());
    previewAction_->setEnabled(!offsets_.empty());
}

void MainWindow::setBusy(bool busy)
{
    analyzeAction_->setEnabled(!busy);
    exportAction_->setEnabled(!busy);
    previewAction_->setEnabled(!busy);
    playAction_->setEnabled(!busy && reader_);
}

void MainWindow::updateTransportUi()
{
    // no-op por ahora
}

PipelineSettings MainWindow::currentSettings() const
{
    PipelineSettings s;
    s.tracker = (trackerCombo_->currentIndex() == 1) ? TrackerType::Centroid
                                                     : TrackerType::Template;
    s.searchFactor = 2.5f;
    s.smoothingAlpha = static_cast<float>(smoothSpin_->value());
    s.borderMode = (borderCombo_->currentIndex() == 1) ? BorderMode::Replicate
                                                       : BorderMode::Black;
    return s;
}

cv::Mat MainWindow::displayFrame(const cv::Mat& src, int64_t frameIndex) const
{
    if (!previewEnabled_ || offsets_.empty())
        return src;
    const int64_t oi = frameIndex - startIndex_;
    if (oi < 0 || oi >= static_cast<int64_t>(offsets_.size()))
        return src;
    return FrameTransformer(currentSettings().borderMode).transform(src, offsets_[oi]);
}

QString MainWindow::formatTime(int64_t us) const
{
    return QTime(0, 0).addMSecs(static_cast<int>(us / 1000)).toString("hh:mm:ss.zzz");
}