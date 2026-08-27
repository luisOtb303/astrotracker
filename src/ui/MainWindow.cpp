#include "ui/MainWindow.h"

#include "common/AppLog.h"
#include "ui/AboutDialog.h"
#include "ui/PhotoPanel.h"
#include "ui/VideoView.h"
#include "video/IVideoReader.h"
#include "video/FFmpegVideoReader.h"
#include "processing/FrameTransformer.h"
#include "export/PipelineWorker.h"

#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QGroupBox>
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
#include <QStandardItemModel>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
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

    const QSettings settings;
    recentProjects_ = settings.value("Projects/recentFiles").toStringList();

    connect(photosPanel_, &PhotoPanel::modified, this, &MainWindow::markProjectModified);
    connect(trackerCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { if (reader_) markProjectModified(); });
    connect(borderCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { if (reader_) markProjectModified(); });
    connect(smoothSpin_, &QDoubleSpinBox::valueChanged, this,
            [this](double) { if (reader_) markProjectModified(); });

    updateStabilizationUi();
    refreshProjectUi();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    setWindowTitle(tr("AstroTracker"));
    resize(1100, 720);

    // Acciones de proyecto, creadas antes de montar el menú para controlar el
    // orden; las mismas QAction se comparten con la barra de herramientas.
    openProjectAction_ = new QAction(style()->standardIcon(QStyle::SP_DirOpenIcon),
                                     tr("Abrir &proyecto..."), this);
    openProjectAction_->setToolTip(tr("Abrir un trabajo guardado (.atracker)"));
    openProjectAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+O")));
    connect(openProjectAction_, &QAction::triggered, this,
            &MainWindow::openProjectDialog);

    closeProjectAction_ = new QAction(tr("&Cerrar proyecto"), this);
    closeProjectAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+W")));
    closeProjectAction_->setEnabled(false);
    closeProjectAction_->setToolTip(tr("Cerrar el proyecto actual y limpiar los resultados"));
    connect(closeProjectAction_, &QAction::triggered, this, &MainWindow::closeProject);

    saveProjectAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton),
                                     tr("&Guardar proyecto"), this);
    saveProjectAction_->setShortcut(QKeySequence::Save);
    saveProjectAction_->setEnabled(false);
    connect(saveProjectAction_, &QAction::triggered, this,
            &MainWindow::saveProjectTriggered);

    saveProjectAsAction_ = new QAction(tr("Guardar proyecto &como..."), this);
    saveProjectAsAction_->setShortcut(QKeySequence::SaveAs);
    saveProjectAsAction_->setEnabled(false);
    connect(saveProjectAsAction_, &QAction::triggered, this,
            &MainWindow::saveProjectAsTriggered);

    QMenu* fileMenu = menuBar()->addMenu(tr("&Archivo"));
    fileMenu->addAction(tr("&Abrir vídeo..."), this, &MainWindow::openFile,
                        QKeySequence::Open);
    fileMenu->addAction(tr("Abrir &fotos (secuencia)..."), this, &MainWindow::openPhotos);
    fileMenu->addAction(openProjectAction_);
    fileMenu->addSeparator();
    fileMenu->addAction(closeProjectAction_);
    fileMenu->addSeparator();
    fileMenu->addAction(saveProjectAction_);
    fileMenu->addAction(saveProjectAsAction_);
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

    QMenu* helpMenu = menuBar()->addMenu(tr("&Ayuda"));
    helpMenu->addAction(tr("&Acerca de AstroTracker..."), this, &MainWindow::showAbout);
    helpMenu->addAction(tr("&Licencias..."), this, &MainWindow::showLicenses);
    helpMenu->addSeparator();
    helpMenu->addAction(tr("Acerca de &Qt"), this, &MainWindow::aboutQt);

    QToolBar* tb = addToolBar(tr("Reproducción"));
    tb->setMovable(false);

    QAction* openTb = tb->addAction(style()->standardIcon(QStyle::SP_DialogOpenButton),
                                    tr("Abrir"), this, &MainWindow::openFile);
    Q_UNUSED(openTb);
    tb->addAction(openProjectAction_);
    tb->addAction(saveProjectAction_);

    playAction_ = tb->addAction(style()->standardIcon(QStyle::SP_MediaPlay),
                                tr("Reproducir/Pausar"), this, &MainWindow::playPause);
    playAction_->setEnabled(false);
    tb->addAction(style()->standardIcon(QStyle::SP_MediaSkipBackward),
                  tr("Atrás"), this, &MainWindow::stepBackward);
    tb->addAction(style()->standardIcon(QStyle::SP_MediaSkipForward),
                  tr("Adelante"), this, &MainWindow::stepForward);
    tb->addAction(style()->standardIcon(QStyle::SP_MediaStop),
                  tr("Detener"), this, &MainWindow::stop);

    // (La toolbar de estabilización desaparece: sus controles viven ahora en
    // el dock "Seguimiento" > Vídeo, junto al resto de ajustes.)

    // Acciones de vídeo: los botones viven en el dock "Seguimiento" > Vídeo.
    analyzeAction_ = new QAction(tr("Seguir"), this);
    analyzeAction_->setToolTip(
        tr("Analizar el vídeo: seguir el objeto y calcular desplazamientos "
           "(dibuja una ROI o deja que detecte el disco automáticamente)"));
    connect(analyzeAction_, &QAction::triggered, this, &MainWindow::startAnalyze);
    previewAction_ = new QAction(tr("Vista previa"), this);
    previewAction_->setCheckable(true);
    previewAction_->setToolTip(tr("Mostrar los frames estabilizados"));
    connect(previewAction_, &QAction::toggled, this, &MainWindow::togglePreview);
    exportAction_ = new QAction(tr("Exportar..."), this);
    exportAction_->setToolTip(tr("Estabilizar y guardar el vídeo de salida"));
    connect(exportAction_, &QAction::triggered, this, &MainWindow::startExport);

    tabs_ = new QTabWidget(this);
    tabs_->addTab(createVideoPage(), tr("Vídeo"));
    photosPanel_ = new PhotoPanel(this);
    tabs_->addTab(photosPanel_, tr("Fotos"));
    setCentralWidget(tabs_);

    connect(photosPanel_, &PhotoPanel::statusMessage, this,
            [this](const QString& msg, int timeoutMs) {
                statusBar()->showMessage(msg, timeoutMs);
                refreshProjectUi();
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

    // Dock "Seguimiento": perfiles y métodos del modo Fotos + ajustes del
    // pipeline de vídeo (antes en la toolbar de estabilización).
    trackDock_ = new QDockWidget(tr("Seguimiento"), this);
    trackDock_->setObjectName(QStringLiteral("trackingDock"));
    trackDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    auto* dockWidget = new QWidget(trackDock_);
    auto* dockLay = new QVBoxLayout(dockWidget);
    dockLay->setContentsMargins(6, 4, 6, 4);

    auto* photosGroup = new QGroupBox(tr("Fotos"), dockWidget);
    auto* photosLay = new QVBoxLayout(photosGroup);
    photosLay->addWidget(new QLabel(tr("Perfil"), photosGroup));
    profileCombo_ = new QComboBox(photosGroup);
    for (int i = 0; i <= static_cast<int>(ObjectProfile::LunarEclipse); ++i) {
        const ObjectProfile op = static_cast<ObjectProfile>(i);
        profileCombo_->addItem(QString::fromUtf8(objectProfileName(op)),
                               static_cast<int>(op));
    }
    photosLay->addWidget(profileCombo_);

    photosLay->addWidget(new QLabel(tr("Método de esta foto"), photosGroup));
    photoMethodCombo_ = new QComboBox(photosGroup);
    photoMethodCombo_->addItem(tr("(según perfil)"),
                               static_cast<int>(DiscMethod::Prediction));
    struct MethodEntry {
        DiscMethod m;
        bool ready;
    };
    const MethodEntry methodEntries[] = {
        {DiscMethod::Template, true},
        {DiscMethod::ArcBlob, true},
        {DiscMethod::KnownRadius, true},
        {DiscMethod::PhaseCorrelation, true},
        {DiscMethod::Centroid, true},
        {DiscMethod::Ecc, true},
        {DiscMethod::Features, true},
    };
    for (const MethodEntry& e : methodEntries) {
        photoMethodCombo_->addItem(QString::fromUtf8(methodName(e.m)),
                                   static_cast<int>(e.m));
        if (!e.ready) {
            if (auto* model = qobject_cast<QStandardItemModel*>(
                    photoMethodCombo_->model())) {
                if (auto* item =
                        model->item(photoMethodCombo_->count() - 1))
                    item->setEnabled(false);
            }
        }
    }
    photosLay->addWidget(photoMethodCombo_);
    photoStatusLabel_ = new QLabel(tr("sin secuencia"), photosGroup);
    photoStatusLabel_->setWordWrap(true);
    photosLay->addWidget(photoStatusLabel_);
    clearOverrideBtn_ = new QPushButton(tr("Quitar método fijado"), photosGroup);
    clearOverrideBtn_->setEnabled(false);
    photosLay->addWidget(clearOverrideBtn_);

    auto* videoGroup = new QGroupBox(tr("Vídeo"), dockWidget);
    auto* videoLay = new QVBoxLayout(videoGroup);
    videoLay->addWidget(new QLabel(tr("Tracker"), videoGroup));
    trackerCombo_ = new QComboBox(videoGroup);
    trackerCombo_->addItem(tr("Template"));
    trackerCombo_->addItem(tr("Centroid"));
    trackerCombo_->addItem(tr("Disco (perfil)"));
    trackerCombo_->setToolTip(tr("Algoritmo de seguimiento. \"Disco (perfil)\" usa "
                                 "el motor del modo Fotos con el perfil elegido arriba"));
    videoLay->addWidget(trackerCombo_);
    videoLay->addWidget(new QLabel(tr("Borde"), videoGroup));
    borderCombo_ = new QComboBox(videoGroup);
    borderCombo_->addItem(tr("Borde negro"));
    borderCombo_->addItem(tr("Borde réplica"));
    borderCombo_->setToolTip(tr("Relleno de los bordes al desplazar el frame"));
    videoLay->addWidget(borderCombo_);
    videoLay->addWidget(new QLabel(tr("Suavizado"), videoGroup));
    smoothSpin_ = new QDoubleSpinBox(videoGroup);
    smoothSpin_->setRange(0.01, 1.0);
    smoothSpin_->setSingleStep(0.05);
    smoothSpin_->setValue(0.3);
    smoothSpin_->setToolTip(tr("Suavizado (alpha EMA) del centro del objeto"));
    videoLay->addWidget(smoothSpin_);

    auto* videoBtnRow = new QHBoxLayout();
    auto* seguirBtn = new QToolButton(videoGroup);
    seguirBtn->setDefaultAction(analyzeAction_);
    auto* previewBtn = new QToolButton(videoGroup);
    previewBtn->setDefaultAction(previewAction_);
    auto* exportBtn = new QToolButton(videoGroup);
    exportBtn->setDefaultAction(exportAction_);
    videoBtnRow->addWidget(seguirBtn);
    videoBtnRow->addWidget(previewBtn);
    videoLay->addLayout(videoBtnRow);
    videoLay->addWidget(exportBtn);

    connect(borderCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { showCurrentFrame(); });

    dockLay->addWidget(photosGroup);
    dockLay->addWidget(videoGroup);
    dockLay->addStretch(1);
    trackDock_->setWidget(dockWidget);
    addDockWidget(Qt::LeftDockWidgetArea, trackDock_);

    connect(profileCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
                const ObjectProfile op = static_cast<ObjectProfile>(
                    profileCombo_->itemData(idx).toInt());
                if (photosPanel_->trackingProfile().profile == op)
                    return;
                photosPanel_->setTrackingProfile(op);
                markProjectModified();
            });
    connect(photoMethodCombo_,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
                const DiscMethod m = static_cast<DiscMethod>(
                    photoMethodCombo_->itemData(idx).toInt());
                if (photosPanel_->overrideFor(photosPanel_->currentIndex()) == m)
                    return;
                photosPanel_->setOverrideForCurrent(m);
                markProjectModified();
            });
    connect(clearOverrideBtn_, &QPushButton::clicked, this, [this] {
        photosPanel_->setOverrideForCurrent(DiscMethod::Prediction);
        markProjectModified();
    });
    connect(photosPanel_, &PhotoPanel::photoStatusChanged, this,
            [this](const QString& s) {
                photoStatusLabel_->setText(s.isEmpty() ? tr("sin secuencia") : s);
                const DiscMethod ov =
                    photosPanel_->overrideFor(photosPanel_->currentIndex());
                clearOverrideBtn_->setEnabled(
                    ov != DiscMethod::Prediction && !photosPanel_->isBusy());
                const int want = photoMethodCombo_->findData(static_cast<int>(ov));
                if (want >= 0 && want != photoMethodCombo_->currentIndex()) {
                    photoMethodCombo_->blockSignals(true);
                    photoMethodCombo_->setCurrentIndex(want);
                    photoMethodCombo_->blockSignals(false);
                }
            });
    connect(photosPanel_, &PhotoPanel::profileChanged, this,
            [this](ObjectProfile p) {
                const int idx = profileCombo_->findData(static_cast<int>(p));
                if (idx >= 0 && idx != profileCombo_->currentIndex()) {
                    profileCombo_->blockSignals(true);
                    profileCombo_->setCurrentIndex(idx);
                    profileCombo_->blockSignals(false);
                }
            });

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

    if (videoFiles.isEmpty() && photoDirs.isEmpty() && recentProjects_.isEmpty()) {
        QAction* none = menu->addAction(tr("(sin recientes)"));
        none->setEnabled(false);
        return;
    }

    if (!recentProjects_.isEmpty()) {
        QAction* title = menu->addAction(tr("Proyectos"));
        title->setEnabled(false);
        for (const QString& p : recentProjects_) {
            QAction* a = menu->addAction(QFileInfo(p).fileName());
            a->setToolTip(p);
            connect(a, &QAction::triggered, this,
                    [this, p] { openProject(p); });
        }
        menu->addSeparator();
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
    refreshProjectUi();
    statusBar()->showMessage(
        tr("Abierto: %1  (%2x%3, %4 fps, %5 frames)  ·  Dibuja una ROI sobre el "
           "objeto o pulsa Seguir (detección automática)")
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
    markProjectModified();
}

void MainWindow::startAnalyze()
{
    if (!reader_ || worker_)
        return;
    const PipelineSettings st = currentSettings();
    if (st.tracker != TrackerType::Disc && roi_.isEmpty()) {
        statusBar()->showMessage(
            tr("Dibuja una ROI sobre el objeto antes de seguir "
               "(el tracker \"Disco\" sí puede detectarlo automáticamente)"), 6000);
        return;
    }

    PipelineWorker::Request req;
    req.mode = PipelineWorker::Mode::Analyze;
    req.inPath = inPath_;
    req.roi = cv::Rect2f(roi_.x(), roi_.y(), roi_.width(), roi_.height());
    req.settings = st;
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
    const bool canTrack = reader_ != nullptr && worker_ == nullptr;
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
    refreshProjectUi();
}

void MainWindow::updateTransportUi()
{
    // no-op por ahora
}

PipelineSettings MainWindow::currentSettings() const
{
    PipelineSettings s;
    const int idx = trackerCombo_->currentIndex();
    s.tracker = (idx == 2)   ? TrackerType::Disc
                : (idx == 1) ? TrackerType::Centroid
                             : TrackerType::Template;
    if (s.tracker == TrackerType::Disc)
        s.profile = photosPanel_->trackingProfile().profile;
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

void MainWindow::showAbout()
{
    AboutDialog dlg(this);
    dlg.showTab(0);
    dlg.exec();
}

void MainWindow::showLicenses()
{
    AboutDialog dlg(this);
    dlg.showTab(1);
    dlg.exec();
}

void MainWindow::aboutQt()
{
    QMessageBox::aboutQt(this);
}

// ---- Proyecto (.atracker): abrir / guardar / guardar como ----

bool MainWindow::anyBusy() const
{
    return photosPanel_->isBusy() || worker_ != nullptr;
}

bool MainWindow::hasContent() const
{
    return photosPanel_->isOpen() || reader_ != nullptr;
}

PhotoProjectVideo MainWindow::collectVideo() const
{
    PhotoProjectVideo v;
    if (!reader_)
        return v;
    v.active = true;
    v.path = inPath_;
    v.hasRoi = !roi_.isEmpty();
    if (v.hasRoi) {
        v.roiX = roi_.x();
        v.roiY = roi_.y();
        v.roiW = roi_.width();
        v.roiH = roi_.height();
    }
    v.startUs = startUs_;
    v.positionUs = currentUs_;
    v.tracker = trackerCombo_->currentIndex();
    v.smoothingAlpha = smoothSpin_->value();
    v.borderMode = borderCombo_->currentIndex();
    return v;
}

void MainWindow::applyVideo(const PhotoProjectVideo& v)
{
    if (!reader_ || inPath_ != v.path) {
        openPath(v.path);
        if (!reader_)
            return;
    }

    trackerCombo_->setCurrentIndex(std::clamp(v.tracker, 0, 1));
    smoothSpin_->setValue(v.smoothingAlpha);
    borderCombo_->setCurrentIndex(std::clamp(v.borderMode, 0, 1));

    // La ROI y el inicio del análisis; los offsets del seguimiento no se
    // guardan (se recalculan con "Seguir").
    roi_ = v.hasRoi ? QRect(v.roiX, v.roiY, v.roiW, v.roiH) : QRect();
    view_->setRoiEnabled(true);
    if (!roi_.isEmpty())
        view_->setRoi(roi_);
    else
        view_->clearRoi();
    offsets_.clear();
    previewEnabled_ = false;
    previewAction_->setChecked(false);
    startUs_ = v.startUs;
    startIndex_ =
        reader_->fps() > 0.0
            ? static_cast<int64_t>(std::llround(startUs_ * reader_->fps() / 1e6))
            : 0;

    if (v.positionUs > 0 && v.positionUs < totalUs_) {
        reader_->seekToUs(v.positionUs);
        showCurrentFrame();
    }
    updateStabilizationUi();
    statusBar()->showMessage(
        tr("Vídeo restaurado: pulsa \"Seguir\" para recalcular el análisis"), 6000);
}

void MainWindow::closeVideo()
{
    timer_->stop();
    playAction_->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    playAction_->setEnabled(false);
    reader_.reset();
    inPath_.clear();
    roi_ = QRect();
    offsets_.clear();
    previewEnabled_ = false;
    previewAction_->setChecked(false);
    view_->setFrame(cv::Mat());
    resultView_->setFrame(cv::Mat());
    view_->clearRoi();
    slider_->setEnabled(false);
    slider_->setValue(0);
    frameLabel_->setText(tr("Frame: - / -"));
    timeLabel_->setText(tr("00:00:00.000"));
    updateStabilizationUi();
}

PhotoProject MainWindow::collectProject() const
{
    PhotoProject p;
    p.savedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    p.photos = photosPanel_->collectState();
    p.video = collectVideo();
    return p;
}

bool MainWindow::saveProject(const QString& path)
{
    if (anyBusy()) {
        statusBar()->showMessage(tr("Espera a que termine la operación antes de guardar"), 5000);
        return false;
    }
    if (!hasContent()) {
        statusBar()->showMessage(tr("No hay nada que guardar"), 5000);
        return false;
    }

    const QByteArray json = photo_project::encode(collectProject());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(this, tr("AstroTracker"),
                              tr("No se pudo guardar el proyecto:\n%1").arg(path));
        AppLog::error(tr("Error al guardar el proyecto: %1").arg(path));
        return false;
    }
    f.write(json);
    f.close();

    projectPath_ = QFileInfo(path).absoluteFilePath();
    projectDirty_ = false;
    rememberProjectPath(projectPath_);
    refreshProjectUi();
    statusBar()->showMessage(tr("Proyecto guardado"), 5000);
    AppLog::info(tr("Proyecto guardado: %1").arg(projectPath_));
    return true;
}

bool MainWindow::saveProjectAs()
{
    QSettings settings;
    QString startDir = settings.value("Photos/lastDir").toString();
    if (startDir.isEmpty() || !QDir(startDir).exists())
        startDir = settings.value("Video/lastDir").toString();

    QString name = QStringLiteral("astrotracker.atracker");
    if (!startDir.isEmpty()) {
        const QString dirName = QDir(startDir).dirName();
        if (!dirName.isEmpty())
            name = dirName + QStringLiteral(".atracker");
    }
    const QString suggested = startDir.isEmpty() ? name : QDir(startDir).filePath(name);

    QString path = QFileDialog::getSaveFileName(
        this, tr("Guardar proyecto"), suggested,
        tr("Proyecto de AstroTracker (*.atracker);;Todos los archivos (*.*)"));
    if (path.isEmpty())
        return false;
    if (!path.endsWith(QStringLiteral(".atracker"), Qt::CaseInsensitive))
        path += QStringLiteral(".atracker");
    return saveProject(path);
}

bool MainWindow::openProject(const QString& path)
{
    if (anyBusy()) {
        QMessageBox::information(this, tr("AstroTracker"),
                                 tr("Espera a que termine el cálculo o la exportación."));
        return false;
    }
    if (!confirmContinue())
        return false;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("AstroTracker"),
                             tr("No se pudo abrir el proyecto:\n%1").arg(path));
        return false;
    }
    PhotoProject p;
    if (!photo_project::decode(f.readAll(), p)) {
        QMessageBox::warning(this, tr("AstroTracker"),
                             tr("El archivo no es un proyecto válido o fue creado por "
                                "una versión más reciente de AstroTracker:\n%1")
                                 .arg(path));
        AppLog::error(tr("Proyecto no válido: %1").arg(path));
        return false;
    }

    bool okPhotos = true;
    if (p.photos.active) {
        QString err;
        okPhotos = photosPanel_->applyState(p.photos, &err);
        if (!okPhotos)
            QMessageBox::warning(this, tr("AstroTracker"), err);
    } else {
        photosPanel_->clearSession();
    }
    if (p.video.active)
        applyVideo(p.video);
    else
        closeVideo();

    tabs_->setCurrentWidget(p.photos.active && okPhotos
                                ? static_cast<QWidget*>(photosPanel_)
                                : static_cast<QWidget*>(tabs_->widget(0)));

    projectPath_ = QFileInfo(path).absoluteFilePath();
    projectDirty_ = false;
    rememberProjectPath(projectPath_);

    // Si hay un autoguardado más reciente que el archivo principal, avisar:
    // contiene trabajo posterior al último "Guardar".
    const QString bak = projectPath_ + QStringLiteral(".bak");
    const QFileInfo mainInfo(projectPath_);
    const QFileInfo bakInfo(bak);
    if (bakInfo.exists() && bakInfo.lastModified() >
            mainInfo.lastModified().addSecs(5))
        AppLog::warn(tr("Hay un autoguardado más reciente que el proyecto (%1); "
                        "renómbralo a .atracker para recuperarlo")
                         .arg(bak));

    refreshProjectUi();
    statusBar()->showMessage(tr("Proyecto abierto"), 5000);
    AppLog::info(tr("Proyecto abierto: %1").arg(projectPath_));
    return true;
}

bool MainWindow::confirmContinue()
{
    if (!projectDirty_)
        return true;

    QMessageBox box(this);
    box.setWindowTitle(tr("AstroTracker"));
    box.setIcon(QMessageBox::Question);
    box.setText(projectPath_.isEmpty()
                    ? tr("¿Guardar el trabajo antes de continuar?")
                    : tr("¿Guardar los cambios en %1?")
                          .arg(QFileInfo(projectPath_).fileName()));
    QPushButton* saveBtn = box.addButton(tr("&Guardar"), QMessageBox::AcceptRole);
    box.addButton(tr("&No guardar"), QMessageBox::DestructiveRole);
    const QPushButton* cancelBtn =
        box.addButton(tr("&Cancelar"), QMessageBox::RejectRole);
    box.setDefaultButton(saveBtn);
    box.exec();

    if (box.clickedButton() == cancelBtn)
        return false;
    if (box.clickedButton() != saveBtn)
        return true;
    return projectPath_.isEmpty() ? saveProjectAs() : saveProject(projectPath_);
}

void MainWindow::writeAutosave()
{
    // Copia de seguridad silenciosa tras cada cambio, una vez que el proyecto
    // tiene archivo. El principal solo se escribe con "Guardar".
    if (projectPath_.isEmpty() || !projectDirty_ || anyBusy())
        return;
    const PhotoProject p = collectProject();
    if (!p.photos.active && !p.video.active)
        return;
    QFile f(projectPath_ + QStringLiteral(".bak"));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    f.write(photo_project::encode(p));
    f.close();
}

void MainWindow::markProjectModified()
{
    projectDirty_ = true;
    refreshProjectUi();
    writeAutosave();
}

void MainWindow::rememberProjectPath(const QString& path)
{
    recentProjects_.removeAll(path);
    recentProjects_.push_front(path);
    while (recentProjects_.size() > 8)
        recentProjects_.removeLast();
    QSettings settings;
    settings.setValue("Projects/recentFiles", recentProjects_);
    settings.setValue("Projects/lastDir", QFileInfo(path).absolutePath());
}

void MainWindow::refreshProjectUi()
{
    const bool content = hasContent();
    const bool busy = anyBusy();
    saveProjectAction_->setEnabled(content && !busy);
    saveProjectAsAction_->setEnabled(content && !busy);
    closeProjectAction_->setEnabled(content && !busy);
    openProjectAction_->setEnabled(!busy);
    profileCombo_->setEnabled(!busy);
    photoMethodCombo_->setEnabled(content && !busy);

    QString title = tr("AstroTracker");
    if (!projectPath_.isEmpty()) {
        title += QStringLiteral(" — ") + QFileInfo(projectPath_).fileName();
        if (projectDirty_)
            title += QLatin1Char('*');
    }
    setWindowTitle(title);
}

void MainWindow::openProjectDialog()
{
    if (anyBusy()) {
        QMessageBox::information(this, tr("AstroTracker"),
                                 tr("Espera a que termine el cálculo o la exportación."));
        return;
    }
    QSettings settings;
    QString startDir = settings.value("Projects/lastDir").toString();
    if (startDir.isEmpty() || !QDir(startDir).exists()) {
        startDir = settings.value("Photos/lastDir").toString();
        if (startDir.isEmpty() || !QDir(startDir).exists())
            startDir = settings.value("Video/lastDir").toString();
    }
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Abrir proyecto"), startDir,
        tr("Proyecto de AstroTracker (*.atracker);;Todos los archivos (*.*)"));
    if (path.isEmpty())
        return;
    openProject(path);
}

void MainWindow::saveProjectTriggered()
{
    if (!hasContent() || anyBusy())
        return;
    if (projectPath_.isEmpty())
        saveProjectAs();
    else
        saveProject(projectPath_);
}

void MainWindow::saveProjectAsTriggered()
{
    if (!hasContent() || anyBusy())
        return;
    saveProjectAs();
}

void MainWindow::closeProject()
{
    if (anyBusy()) {
        QMessageBox::information(this, tr("AstroTracker"),
                                 tr("Espera a que termine el cálculo o la exportación."));
        return;
    }
    if (!hasContent())
        return;
    if (!confirmContinue())
        return;

    photosPanel_->clearSession();
    closeVideo();
    projectPath_.clear();
    projectDirty_ = false;
    refreshProjectUi();
    statusBar()->showMessage(tr("Proyecto cerrado"), 5000);
    AppLog::info(tr("Proyecto cerrado"));
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (anyBusy()) {
        QMessageBox::information(this, tr("AstroTracker"),
                                 tr("Hay un cálculo o una exportación en curso. "
                                    "Detenla (botón \"Detener\") antes de cerrar."));
        event->ignore();
        return;
    }
    if (!confirmContinue()) {
        event->ignore();
        return;
    }
    event->accept();
}