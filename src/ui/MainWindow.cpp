#include "ui/MainWindow.h"

#include "common/AppLog.h"
#include "ui/DebugDep.h"
#include "ui/AboutDialog.h"
#include "ui/PhotoPanel.h"
#include "ui/VideoView.h"
#include "ui/theme/ThemeManager.h"
#include "ui/shortcuts/ShortcutManager.h"
#include "ui/panels/InfoPanel.h"
#include "ui/timeline/TimelineWidget.h"
#include "video/IVideoReader.h"
#include "video/FFmpegVideoReader.h"
#include "stills/VideoExportWorker.h"
#include "processing/FrameTransformer.h"
#include "tracking/DiscArcFit.h"
#include "export/PipelineWorker.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColor>
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
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QLineEdit>
#include <QProgressBar>
#include <QRadioButton>
#include <QSettings>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QTime>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    // Apply dark theme by default.
    ThemeManager::instance().setTheme(AppTheme::Dark);

    setupUi();
    timer_ = new QTimer(this);
    timer_->setInterval(33);
    connect(timer_, &QTimer::timeout, this, &MainWindow::onTimer);
    connect(view_, &VideoView::roiSelected, this, &MainWindow::onRoiSelected);

    const QSettings settings;
    recentProjects_ = settings.value("Projects/recentFiles").toStringList();

    connect(photosPanel_, &PhotoPanel::modified, this, &MainWindow::markProjectModified);
    connect(photosPanel_, &PhotoPanel::photoMetaChanged,
            this, [this](const PhotoFileInfo& file, const PhotoExifInfo& exif) {
                depLog(QStringLiteral("metaChanged name=%1 size=%2 hasCam=%3 hasExif=%4")
                           .arg(QString::fromStdString(file.name))
                           .arg(file.sizeBytes)
                           .arg(exif.hasCamera())
                           .arg(QString::fromStdString(file.type)));
                infoPanel_->setFileAndExif(file, exif);
            });
    connect(photosPanel_, &PhotoPanel::photoPositionChanged, this,
            [this](int64_t index) {
                if (tabs_->currentIndex() == 1)
                    updateTimelineForPhotos(index);
            });
    connect(trackerCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { if (reader_) markProjectModified(); });
    connect(borderCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { if (reader_) markProjectModified(); });
    connect(smoothSpin_, &QDoubleSpinBox::valueChanged, this,
            [this](double) { if (reader_) markProjectModified(); });

    installShortcuts();

    updateStabilizationUi();
    refreshProjectUi();
    updatePanelMode();

    // Restaurar geometría/estado de docks guardado.
    const QSettings uiSettings;
    const QByteArray geometry = uiSettings.value("UI/geometry").toByteArray();
    const QByteArray state = uiSettings.value("UI/state").toByteArray();
    if (!geometry.isEmpty() && !restoreGeometry(geometry))
        resize(1280, 800);
    if (!state.isEmpty())
        restoreState(state);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    setWindowTitle(tr("AstroTracker"));
    resize(1280, 800);

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

    trackDockAction_ = viewMenu->addAction(tr("&Seguimiento"));
    trackDockAction_->setCheckable(true);
    trackDockAction_->setChecked(true);
    connect(trackDockAction_, &QAction::toggled, this, [this](bool on) {
        if (trackDock_)
            trackDock_->setVisible(on);
    });

    infoDockAction_ = viewMenu->addAction(tr("&Información"));
    infoDockAction_->setCheckable(true);
    infoDockAction_->setChecked(true);
    infoDockAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+I")));
    connect(infoDockAction_, &QAction::toggled, this, [this](bool on) {
        if (infoDock_)
            infoDock_->setVisible(on);
    });
    viewMenu->addSeparator();
    QAction* restorePanels = viewMenu->addAction(tr("&Restablecer paneles"));
    restorePanels->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+R")));
    connect(restorePanels, &QAction::triggered, this, &MainWindow::restoreDocks);

    QMenu* helpMenu = menuBar()->addMenu(tr("&Ayuda"));
    helpMenu->addAction(tr("&Acerca de AstroTracker..."), this, &MainWindow::showAbout);
    helpMenu->addAction(tr("&Licencias..."), this, &MainWindow::showLicenses);
    helpMenu->addSeparator();
    helpMenu->addAction(tr("Acerca de &Qt"), this, &MainWindow::aboutQt);

    // Toolbar "Archivo" (siempre visible).
    auto* fileToolBar = addToolBar(tr("Archivo"));
    fileToolBar->setMovable(false);

    QAction* openTb = fileToolBar->addAction(
        style()->standardIcon(QStyle::SP_DialogOpenButton),
        tr("Abrir"), this, &MainWindow::openFile);
    Q_UNUSED(openTb);
    fileToolBar->addAction(openProjectAction_);
    fileToolBar->addAction(saveProjectAction_);

    // Toolbar "Vídeo" (solo visible con vídeo cargado en pestaña Vídeo).
    transportToolBar_ = addToolBar(tr("Vídeo"));
    transportToolBar_->setMovable(false);
    playAction_ = transportToolBar_->addAction(
        style()->standardIcon(QStyle::SP_MediaPlay),
        tr("Reproducir/Pausar"), this, &MainWindow::playPause);
    playAction_->setEnabled(false);
    transportToolBar_->addAction(
        style()->standardIcon(QStyle::SP_MediaSkipBackward),
        tr("Atrás"), this, &MainWindow::stepBackward);
    transportToolBar_->addAction(
        style()->standardIcon(QStyle::SP_MediaSkipForward),
        tr("Adelante"), this, &MainWindow::stepForward);
    transportToolBar_->addAction(
        style()->standardIcon(QStyle::SP_MediaStop),
        tr("Detener"), this, &MainWindow::stop);
    transportToolBar_->hide();

    // Acciones de vídeo: viven en la toolbar de la pestaña Vídeo.
    analyzeAction_ = new QAction(tr("Calcular automáticamente"), this);
    analyzeAction_->setToolTip(
        tr("Seguir el objeto en todo el vídeo (si el tracker es \"Disco\", "
           "detecta el disco automáticamente; con los otros dibuja una ROI "
           "antes). Cada frame se desplaza para mantener el objeto centrado."));
    connect(analyzeAction_, &QAction::triggered, this, &MainWindow::startAnalyze);
    fitAction_ = new QAction(tr("Ajustar fotograma"), this);
    fitAction_->setToolTip(
        tr("Detectar el disco solo en el fotograma actual y señalar su "
           "posición: el punto de partida del cálculo automático"));
    connect(fitAction_, &QAction::triggered, this, &MainWindow::onFitFrame);
    previewAction_ = new QAction(tr("Vista previa"), this);
    previewAction_->setCheckable(true);
    previewAction_->setToolTip(tr("Mostrar el fotograma centrado en el visor derecho"));
    connect(previewAction_, &QAction::toggled, this, &MainWindow::togglePreview);
    exportVideoAction_ = new QAction(tr("Exportar vídeo..."), this);
    exportVideoAction_->setToolTip(tr("Guardar el vídeo como MP4 (centrado si se ha "
                                      "calculado, directo si no)"));
    connect(exportVideoAction_, &QAction::triggered, this, &MainWindow::startExportVideo);
    exportPhotosAction_ = new QAction(tr("Exportar fotos..."), this);
    exportPhotosAction_->setToolTip(tr("Extraer los fotogramas del vídeo como imágenes "
                                       "PNG/JPG (centradas si se han calculado, directas "
                                       "si no)"));
    connect(exportPhotosAction_, &QAction::triggered, this, &MainWindow::startExportPhotos);
    stopAction_ = new QAction(tr("Detener"), this);
    stopAction_->setEnabled(false);
    stopAction_->setToolTip(tr("Detener el cálculo/exportación en curso"));
    connect(stopAction_, &QAction::triggered, this, &MainWindow::stopProcessing);

    // --- Central: tabs (Vídeo|Fotos) + timeline ---
    auto* centralWidget = new QWidget(this);
    auto* centerLayout = new QVBoxLayout(centralWidget);
    centerLayout->setContentsMargins(4, 4, 4, 4);
    centerLayout->setSpacing(4);

    tabs_ = new QTabWidget(this);
    tabs_->addTab(createVideoPage(), tr("Vídeo"));
    photosPanel_ = new PhotoPanel(this);
    tabs_->addTab(photosPanel_, tr("Fotos"));
    centerLayout->addWidget(tabs_, 1);

    // Timeline at the bottom of center
    timeline_ = new TimelineWidget(this);
    centerLayout->addWidget(timeline_);

    connect(timeline_, &TimelineWidget::valueChanged, this, [this](int val) {
        // En el modo Fotos el timeline navega por fotos (preview del TL); en
        // el modo Vídeo sigue haciendo seek por milisegundos.
        if (tabs_->currentIndex() == 1) {
            if (photosPanel_ && photosPanel_->isOpen() &&
                static_cast<int64_t>(val) != photosPanel_->currentIndex())
                photosPanel_->showPhoto(val);
            return;
        }
        if (!reader_ || val == currentUs_ / 1000)
            return;
        reader_->seekToUs(static_cast<int64_t>(val) * 1000);
        showCurrentFrame();
    });

    setCentralWidget(centralWidget);

    // Right dock: Información (estadísticas de seguimiento + info de frame + EXIF).
    infoPanel_ = new InfoPanel(this);
    infoDock_ = new QDockWidget(tr("Información"), this);
    infoDock_->setObjectName(QStringLiteral("infoDock"));
    infoDock_->setMinimumWidth(220);
    infoDock_->setWidget(infoPanel_);
    infoDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, infoDock_);
    connect(infoDock_, &QDockWidget::visibilityChanged, infoDockAction_,
            &QAction::setChecked);

    connect(tabs_, &QTabWidget::currentChanged, this, [this]() {
        updateTransportUi();
        updatePanelMode();
    });
    updateTransportUi();

    connect(photosPanel_, &PhotoPanel::statusMessage, this,
            [this](const QString& msg, int timeoutMs) {
                statusBar()->showMessage(msg, timeoutMs);
                refreshProjectUi();
                updatePanelMode();
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
    connect(logDock_, &QDockWidget::visibilityChanged, logDockAction_,
            &QAction::setChecked);

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

    videoGroup_ = new QGroupBox(tr("Vídeo"), dockWidget);
    auto* videoLay = new QVBoxLayout(videoGroup_);
    videoLay->addWidget(new QLabel(tr("Tracker"), videoGroup_));
    trackerCombo_ = new QComboBox(videoGroup_);
    trackerCombo_->addItem(tr("Template"));
    trackerCombo_->addItem(tr("Centroid"));
    trackerCombo_->addItem(tr("Disco (perfil)"));
    trackerCombo_->setCurrentIndex(2); // Disco: detecta solo, sin ROI
    trackerCombo_->setToolTip(tr("Algoritmo de seguimiento. \"Disco (perfil)\" usa "
                                 "el motor del modo Fotos con el perfil elegido arriba "
                                 "y detecta el disco automáticamente"));
    videoLay->addWidget(trackerCombo_);
    videoLay->addWidget(new QLabel(tr("Borde"), videoGroup_));
    borderCombo_ = new QComboBox(videoGroup_);
    borderCombo_->addItem(tr("Borde negro"));
    borderCombo_->addItem(tr("Borde réplica"));
    borderCombo_->setToolTip(tr("Relleno de los bordes al desplazar el frame"));
    videoLay->addWidget(borderCombo_);
    videoLay->addWidget(new QLabel(tr("Suavizado"), videoGroup_));
    smoothSpin_ = new QDoubleSpinBox(videoGroup_);
    smoothSpin_->setRange(0.01, 1.0);
    smoothSpin_->setSingleStep(0.05);
    smoothSpin_->setValue(0.3);
    smoothSpin_->setToolTip(tr("Suavizado (alpha EMA) del centro del objeto"));
    videoLay->addWidget(smoothSpin_);

    connect(borderCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { showCurrentFrame(); });

    dockLay->addWidget(photosGroup);
    dockLay->addWidget(videoGroup_);
    dockLay->addStretch(1);
    trackDock_->setWidget(dockWidget);
    addDockWidget(Qt::LeftDockWidgetArea, trackDock_);
    connect(trackDock_, &QDockWidget::visibilityChanged, trackDockAction_,
            &QAction::setChecked);

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

}

QWidget* MainWindow::createVideoPage()
{
    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(4);

    // Toolbar embebida de la pestaña Vídeo.
    auto* toolbar = new QToolBar(central);
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    toolbar->setStyleSheet(QStringLiteral("QToolBar { border: 0; }"));
    auto* openVideo = toolbar->addAction(
        style()->standardIcon(QStyle::SP_DialogOpenButton),
        tr("Abrir vídeo..."), this, &MainWindow::openFile);
    Q_UNUSED(openVideo);
    toolbar->addAction(
        style()->standardIcon(QStyle::SP_DialogOpenButton),
        tr("Abrir fotos..."), this, &MainWindow::openPhotos);
    toolbar->addSeparator();
    toolbar->addAction(analyzeAction_);
    toolbar->addAction(fitAction_);
    toolbar->addAction(previewAction_);
    toolbar->addAction(exportVideoAction_);
    toolbar->addAction(exportPhotosAction_);
    toolbar->addSeparator();
    toolbar->addAction(stopAction_);
    root->addWidget(toolbar);

    auto* viewers = new QHBoxLayout();

    auto* sourceBox = new QVBoxLayout();
    auto* srcTitle = new QLabel(tr("Original"), central);
    srcTitle->setAlignment(Qt::AlignCenter);
    srcTitle->setToolTip(
        tr("El vídeo original. Después de calcular, se marca sobre el objeto "
           "lo que se está siguiendo para que se entienda el centrado."));
    sourceBox->addWidget(srcTitle);
    view_ = new VideoView(central);
    sourceBox->addWidget(view_, 1);

    auto* resultBox = new QVBoxLayout();
    auto* resTitle = new QLabel(tr("Centrado"), central);
    resTitle->setAlignment(Qt::AlignCenter);
    resTitle->setToolTip(
        tr("Cada frame se desplaza (y se rellenan los bordes) para que el "
           "objeto se mantenga fijo, centrado en el fotograma."));
    resultBox->addWidget(resTitle);
    resultView_ = new VideoView(central);
    resultView_->setRoiEnabled(false);
    resultBox->addWidget(resultView_, 1);

    viewers->addLayout(sourceBox, 1);
    viewers->addLayout(resultBox, 1);
    root->addLayout(viewers, 1);

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
    {
        const QFileInfo fi(inPath_);
        videoFileName_ = fi.fileName();
        videoFilePath_ = fi.absoluteFilePath();
        videoFileSize_ = fi.size();
        videoFileDate_ = fi.lastModified().toString(Qt::ISODate);
    }
    if (reader_->fps() > 0.0)
        stepUs_ = static_cast<int64_t>(1000000.0 / reader_->fps());
    currentUs_ = 0;
    roi_ = QRect();
    offsets_.clear();
    trackSamples_.clear();
    currentFrameImage_ = cv::Mat();
    hasSeed_ = false;
    previewEnabled_ = false;
    previewAction_->setChecked(false);
    previewAction_->setEnabled(false);
    view_->setRoiEnabled(true);
    view_->clearRoi();
    view_->clearCircle();
    resultView_->clearCircle();
    startUs_ = 0;
    startIndex_ = 0;

    // Update timeline
    timeline_->setRange(0, static_cast<int>(totalUs_ / 1000));
    timeline_->setValue(0);
    timeline_->setEnabled(true);
    playAction_->setEnabled(true);

    showCurrentFrame();
    updateTransportUi();
    updateStabilizationUi();
    updatePanelMode();
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
    presentFrame(frame);
}

void MainWindow::presentFrame(const Frame& frame)
{
    currentUs_ = frame.ptsUs;
    if (!frame.image.empty())
        currentFrameImage_ = frame.image.clone();
    view_->setFrame(frame.image);
    resultView_->setFrame(displayFrame(frame.image, frame.index));
    updateTrackCircle(frame.index);

    const int ms = static_cast<int>(currentUs_ / 1000);
    timeline_->setValue(ms);
    timeline_->setFrameLabel(static_cast<int>(frame.index), static_cast<int>(totalFrames_));
    timeline_->setTimeLabel(formatTime(currentUs_));

    // Update info panel
    if (infoPanel_) {
        const QString time = formatTime(currentUs_);
        infoPanel_->setFrameInfo(
            static_cast<int>(frame.index),
            static_cast<int>(totalFrames_),
            time,
            trackerCombo_ ? trackerCombo_->currentText() : QString(),
            0.0f, 0.0f, 0.0f,
            offsets_.empty() ? tr("No tracking") : tr("Tracking active"));
        infoPanel_->setVideoFileInfo(videoFileName_, videoFilePath_,
                                     videoFileSize_, videoFileDate_,
                                     static_cast<int>(frame.index),
                                     static_cast<int>(totalFrames_));
    }
}

void MainWindow::updateTrackCircle(int64_t frameIndex)
{
    const int64_t oi = frameIndex - startIndex_;
    if (oi >= 0 && oi < static_cast<int64_t>(trackSamples_.size())) {
        const TrackSample& s = trackSamples_.at(static_cast<int>(oi));
        view_->setCircle(QPointF(s.center.x, s.center.y), s.radius,
                         s.predicted || s.status == TrackStatus::LOST);
    } else if (hasSeed_) {
        view_->setCircle(QPointF(seedCenter_.x(), seedCenter_.y()), seedRadius_, false);
    } else {
        view_->clearCircle();
    }
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
    presentFrame(frame);
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
            tr("Dibuja una ROI sobre el objeto o pulsa \"Ajustar fotograma\" "
               "antes de calcular (o usa el tracker \"Disco\", que lo detecta "
               "automáticamente)"), 6000);
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

void MainWindow::onFitFrame()
{
    if (!reader_ || worker_ || currentFrameImage_.empty())
        return;

    cv::Mat gray;
    cv::cvtColor(currentFrameImage_, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, cv::Size(0, 0), 1.2);
    const cv::Rect full(0, 0, gray.cols, gray.rows);
    cv::Point2f prior(gray.cols * 0.5f, gray.rows * 0.5f);
    float guess = std::max(20.f, 0.10f * gray.cols);
    cv::Point2f blobC;
    int area = 0, bw = 0, bh = 0;
    if (DiscArcFit::blobInfo(gray, full, blobC, area, bw, bh) &&
        area > 100 && bw > 8 && bh > 8) {
        const float aspect = static_cast<float>(bw) / static_cast<float>(bh);
        if (aspect > 0.4f && aspect < 2.5f) {
            guess = std::clamp(std::sqrt(static_cast<float>(area) / static_cast<float>(CV_PI)),
                               10.f, 0.45f * std::min(gray.cols, gray.rows));
            prior = blobC;
        }
    }
    const DiscArcEstimate seed = DiscArcFit::fitDisc(gray, full, prior, guess, 3.f);
    if (!seed.ok || seed.radius <= 0.f ||
        cv::norm(seed.center - prior) > 2.0f * guess) {
        statusBar()->showMessage(
            tr("No se ha encontrado un disco creíble en este fotograma. "
               "Prueba en otro momento del vídeo (dibuja una ROI si el disco "
               "es muy pequeño)"), 6000);
        return;
    }

    const float side = seed.radius * 3.0f;
    QRect roi(qRound(seed.center.x - side * 0.5f),
              qRound(seed.center.y - side * 0.5f),
              qRound(side), qRound(side));
    roi = roi.intersected(QRect(0, 0, currentFrameImage_.cols, currentFrameImage_.rows));
    if (roi.width() < 4 || roi.height() < 4) {
        statusBar()->showMessage(tr("El disco detectado es demasiado pequeño"), 6000);
        return;
    }

    roi_ = roi;
    view_->setRoi(roi);
    seedCenter_ = QPointF(seed.center.x, seed.center.y);
    seedRadius_ = seed.radius;
    hasSeed_ = true;
    view_->setCircle(seedCenter_, seedRadius_, false);
    startUs_ = currentUs_;
    startIndex_ = reader_ ? static_cast<int64_t>(std::llround(currentUs_ * reader_->fps() / 1e6)) : 0;
    statusBar()->showMessage(
        tr("Disco detectado: centro (%1,%2), radio %3 px. Pulsa \"Calcular "
           "automáticamente\" para seguirlo en todo el vídeo")
            .arg(seed.center.x)
            .arg(seed.center.y)
            .arg(seed.radius),
        6000);
    updateStabilizationUi();
    markProjectModified();
}

void MainWindow::stopProcessing()
{
    if (worker_)
        worker_->requestInterruption();
    if (videoExportWorker_)
        videoExportWorker_->requestStop();
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

void MainWindow::startExportVideo()
{
    runExportDialog(true);
}

void MainWindow::startExportPhotos()
{
    runExportDialog(false);
}

void MainWindow::runExportDialog(bool toVideo)
{
    if (!reader_ || worker_ || videoExportWorker_)
        return;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Exportar vídeo"));
    auto* lay = new QVBoxLayout(&dlg);

    auto* centringGroup = new QGroupBox(tr("Contenido"), &dlg);
    auto* centringLay = new QVBoxLayout(centringGroup);
    auto* directRadio = new QRadioButton(tr("Directo (sin centrar)"), &dlg);
    auto* centeredRadio = new QRadioButton(tr("Centrado (seguimiento)"), &dlg);
    const bool canCenter = !offsets_.empty();
    directRadio->setToolTip(tr("Re-codifica el vídeo tal cual, sin desplazar"));
    centeredRadio->setToolTip(tr("Desplaza cada fotograma para dejar el objeto "
                                 "fijo en el centro"));
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
    jpgRadio->setToolTip(tr("Un imagen por fotograma del vídeo"));
    pngRadio->setToolTip(tr("Un imagen por fotograma (sin pérdida)"));
    mp4Radio->setToolTip(tr("Un vídeo H.264 (MP4) con todos los fotogramas"));
    lay->addWidget(jpgRadio);
    lay->addWidget(pngRadio);
    lay->addWidget(mp4Radio);

    auto* resLabel = new QLabel(tr("Resolución"), &dlg);
    lay->addWidget(resLabel);
    auto* resCombo = new QComboBox(&dlg);
    resCombo->addItem(tr("Original"),
                      static_cast<int>(VideoExportWorker::Resolution::Original));
    resCombo->addItem(tr("HD (1280×720)"),
                      static_cast<int>(VideoExportWorker::Resolution::HD));
    resCombo->addItem(tr("FHD (1920×1080)"),
                      static_cast<int>(VideoExportWorker::Resolution::FHD));
    resCombo->addItem(tr("2K (2560×1440)"),
                      static_cast<int>(VideoExportWorker::Resolution::QHD));
    resCombo->addItem(tr("4K (3840×2160)"),
                      static_cast<int>(VideoExportWorker::Resolution::UHD));
    resCombo->setCurrentIndex(0);
    lay->addWidget(resCombo);

    auto* fpsLabel = new QLabel(tr("FPS (solo vídeo)"), &dlg);
    lay->addWidget(fpsLabel);
    auto* fpsCombo = new QComboBox(&dlg);
    fpsCombo->setEditable(true);
    if (reader_)
        fpsCombo->addItem(QString::number(reader_->fps(), 'f', 1));
    fpsCombo->addItems({QStringLiteral("10"), QStringLiteral("5"), QStringLiteral("24"),
                        QStringLiteral("30")});
    fpsCombo->setCurrentIndex(reader_ ? 0 : 1);
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
    interpCombo->setToolTip(tr("Fotogramas generados entre cada par de fotogramas "
                               "para que la transición no sea brusca"));
    interpRow->addWidget(new QLabel(tr("Fotogramas intermedios"), &dlg));
    interpRow->addWidget(interpCombo, 1);
    smoothLay->addLayout(interpRow);
    auto* brightChk = new QCheckBox(tr("Normalizar brillo entre fotogramas"), &dlg);
    brightChk->setChecked(true);
    brightChk->setToolTip(tr("Escala el brillo de cada fotograma al del primero para "
                             "evitar el parpadeo"));
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
                this, tr("Vídeo de salida"), inPath_ + QStringLiteral(".centrado.mp4"),
                tr("MP4 (*.mp4)"));
            if (!f.isEmpty())
                destEdit->setText(f);
        } else {
            const QString dir = QFileDialog::getExistingDirectory(
                this, tr("Carpeta de salida"), inPath_);
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

    VideoExportWorker::Settings st;
    if (mp4Radio->isChecked()) {
        st.format = VideoExportWorker::Format::Mp4;
        st.outFile = destEdit->text();
        bool okFps = false;
        const double fps = fpsCombo->currentText().toDouble(&okFps);
        st.fps = (okFps && fps > 0.0) ? fps : 10.0;
    } else {
        st.format = pngRadio->isChecked() ? VideoExportWorker::Format::Png
                                          : VideoExportWorker::Format::Jpg;
        st.outDir = destEdit->text();
    }
    st.resolution = static_cast<VideoExportWorker::Resolution>(resCombo->currentData().toInt());
    st.borderMode = borderCombo_->currentIndex();
    st.interp = interpCombo->currentData().toInt();
    st.normalizeBrightness = brightChk->isChecked();

    const std::vector<cv::Point2f> exportOffsets =
        centeredRadio->isChecked() ? offsets_ : std::vector<cv::Point2f>{};

    videoExportWorker_ = new VideoExportWorker(inPath_, exportOffsets, st, this);
    connect(videoExportWorker_, &VideoExportWorker::progress, this,
            &MainWindow::onWorkerProgress);
    connect(videoExportWorker_, &VideoExportWorker::frameProcessed, this,
            &MainWindow::onVideoExportFrame);
    connect(videoExportWorker_, &VideoExportWorker::finished, this,
            &MainWindow::onVideoExportFinished);
    connect(videoExportWorker_, &QThread::finished, videoExportWorker_, &QObject::deleteLater);

    lastShownExportIndex_ = -1;
    reader_->seekToUs(startUs_);
    setBusy(true);
    progressBar_->setRange(0, static_cast<int>(reader_->frameCount()));
    progressBar_->setValue(0);
    progressBar_->setVisible(true);
    statusBar()->showMessage(tr("Exportando secuencia..."));
    videoExportWorker_->start();
}

void MainWindow::onVideoExportFrame(int64_t index)
{
    // El worker del export usa su propio lector; aquí se avanza el lector de la
    // UI (libre durante el export) para mostrar en el visor "Centrado" el frame
    // tal y como se está escribiendo. Solo lectura: no altera el resultado.
    if (!reader_ || index < 0)
        return;

    Frame frame;
    if (index == lastShownExportIndex_ + 1) {
        // Avance secuencial normal: no hace falta re-buscar.
        if (!reader_->readNext(frame))
            return;
    } else {
        // Salto (p. ej. cambio de configuración): re-buscar el índice.
        const double fps = reader_->fps();
        if (fps <= 0.0)
            return;
        reader_->seekToUs(static_cast<int64_t>(index * 1e6 / fps));
        if (!reader_->readNext(frame))
            return;
        if (frame.index != index)
            return;
    }
    lastShownExportIndex_ = index;
    presentFrame(frame);
}

void MainWindow::onVideoExportFinished(bool ok, const QString& error, int frames)
{
    videoExportWorker_ = nullptr;
    setBusy(false);
    progressBar_->setVisible(false);
    if (ok)
        statusBar()->showMessage(tr("Exportado: %1 (%2 elementos)").arg(inPath_).arg(frames));
    else
        statusBar()->showMessage(tr("Error al exportar: %1").arg(error));
    updateStabilizationUi();
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
                                   QVector<TrackSample> samples,
                                   int frames, int valid, double meanConfidence)
{
    Q_UNUSED(error);
    if (ok && !offsets.isEmpty()) {
        offsets_.clear();
        offsets_.reserve(static_cast<size_t>(offsets.size()));
        for (const QPointF& p : offsets)
            offsets_.push_back(cv::Point2f(static_cast<float>(p.x()),
                                           static_cast<float>(p.y())));
        trackSamples_ = samples;
        hasSeed_ = false;
        previewEnabled_ = true;
        previewAction_->setChecked(true);
        view_->setRoiEnabled(false);
        showCurrentFrame();
        statusBar()->showMessage(
            tr("Seguimiento: %1 frames, %2 válidos, %3% confianza. "
               "El objeto queda fijo y centrado en el visor \"Centrado\"")
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

    // Elige colores legibles según el tema del sistema (claro u oscuro).
    static const bool dark = [] {
        const QColor bg = QApplication::palette().window().color();
        return bg.lightness() < 128;
    }();
    const QString infoC  = dark ? QStringLiteral("#e8e8e8") : QStringLiteral("#222");
    const QString warnC  = dark ? QStringLiteral("#e6b64c") : QStringLiteral("#a06000");
    const QString errC   = dark ? QStringLiteral("#ff6b6b") : QStringLiteral("#c00");
    const QString debugC = dark ? QStringLiteral("#9a9a9a") : QStringLiteral("#888");

    QString color;
    QString tag;
    switch (level) {
    case AppLog::Error:
        color = errC;
        tag = tr("[error]");
        break;
    case AppLog::Warn:
        color = warnC;
        tag = tr("[aviso]");
        break;
    case AppLog::Debug:
        color = debugC;
        tag = tr("[debug]");
        break;
    default:
        color = infoC;
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
    fitAction_->setEnabled(canTrack);
    const bool canExport = canTrack;
    exportVideoAction_->setEnabled(canExport);
    exportPhotosAction_->setEnabled(canExport);
    previewAction_->setEnabled(!offsets_.empty());
}

void MainWindow::setBusy(bool busy)
{
    analyzeAction_->setEnabled(!busy);
    fitAction_->setEnabled(!busy);
    exportVideoAction_->setEnabled(!busy);
    exportPhotosAction_->setEnabled(!busy);
    previewAction_->setEnabled(!busy);
    stopAction_->setEnabled(busy);
    playAction_->setEnabled(!busy && reader_);
    refreshProjectUi();
}

void MainWindow::updateTransportUi()
{
    const bool onVideoTab = tabs_->currentIndex() == 0;
    const bool hasVideo = reader_ != nullptr;

    // Toolbar de vídeo: solo en pestaña Vídeo con vídeo cargado.
    transportToolBar_->setVisible(onVideoTab && hasVideo);

    // Grupo "Vídeo" del dock: solo en pestaña Vídeo.
    if (videoGroup_)
        videoGroup_->setVisible(onVideoTab);
}

void MainWindow::restoreDocks()
{
    if (trackDock_)
        trackDock_->setVisible(true);
    if (logDock_)
        logDock_->setVisible(true);
    if (infoDock_)
        infoDock_->setVisible(true);
    if (trackDock_)
        addDockWidget(Qt::LeftDockWidgetArea, trackDock_);
    if (logDock_)
        addDockWidget(Qt::BottomDockWidgetArea, logDock_);
    if (infoDock_)
        addDockWidget(Qt::RightDockWidgetArea, infoDock_);
    if (logDock_)
        resizeDocks({logDock_}, {160}, Qt::Vertical);
}

void MainWindow::installShortcuts()
{
    shortcuts_ = new ShortcutManager(this);
    shortcuts_->install(this);
    connect(shortcuts_, &ShortcutManager::triggered, this,
            [this](ShortcutManager::Action a) {
                switch (a) {
                case ShortcutManager::Action::OpenFile:
                    openFile();
                    break;
                case ShortcutManager::Action::PlayPause:
                    playPause();
                    break;
                case ShortcutManager::Action::Stop:
                    stop();
                    break;
                case ShortcutManager::Action::StepForward:
                case ShortcutManager::Action::NextFrame:
                    stepForward();
                    break;
                case ShortcutManager::Action::StepBack:
                case ShortcutManager::Action::PrevFrame:
                    stepBackward();
                    break;
                case ShortcutManager::Action::StartAnalyze:
                    startAnalyze();
                    break;
                case ShortcutManager::Action::TogglePreview:
                    togglePreview(!previewEnabled_);
                    break;
                case ShortcutManager::Action::StartExport:
                    startExportVideo();
                    break;
                case ShortcutManager::Action::ToggleInfo:
                    infoDock_->setVisible(!infoDock_->isVisible());
                    break;
                case ShortcutManager::Action::ZoomIn:
                    view_->zoomIn();
                    resultView_->zoomIn();
                    break;
                case ShortcutManager::Action::ZoomOut:
                    view_->zoomOut();
                    resultView_->zoomOut();
                    break;
                case ShortcutManager::Action::ZoomFit:
                    view_->setZoomFit();
                    resultView_->setZoomFit();
                    break;
                case ShortcutManager::Action::Zoom100:
                    view_->setZoomPercent(100);
                    resultView_->setZoomPercent(100);
                    break;
                default:
                    break;
                }
            });
}

void MainWindow::updatePanelMode()
{
    const bool onVideoTab = tabs_->currentIndex() == 0;
    const bool hasContent = reader_ != nullptr || photosPanel_->isOpen();
    const bool onPhotoTab = !onVideoTab;

    if (infoPanel_) {
        depLog(QStringLiteral("updatePanelMode onPhoto=%1 hasContent=%2 isOpen=%3")
                   .arg(onPhotoTab).arg(hasContent).arg(photosPanel_->isOpen()));
        infoPanel_->setTrackingVisible(hasContent && onVideoTab);
        infoPanel_->setFrameInfoVisible(hasContent && onVideoTab);
        // "Archivo y metadatos" se muestra en ambos modos con contenido:
        // en Fotos = fichero + EXIF; en Vídeo = fichero + frame.
        infoPanel_->setFileInfoVisible(hasContent);
        if (!onPhotoTab || !photosPanel_->isOpen())
            infoPanel_->clearFileInfo();
    }

    // Timeline inferior: en el modo Fotos lo usa el preview del timelapse
    // (frame 1/N y el tiempo actual/total según el fps del preview).
    if (onPhotoTab)
        updateTimelineForPhotos();
}

void MainWindow::updateTimelineForPhotos(int64_t index)
{
    if (!timeline_)
        return;
    if (!photosPanel_ || !photosPanel_->isOpen() || photosPanel_->count() < 1) {
        timeline_->setEnabled(false);
        return;
    }
    if (index < 0)
        index = photosPanel_->currentIndex();
    const int64_t n = photosPanel_->count();
    timeline_->setRange(0, static_cast<int>(n - 1));
    timeline_->setEnabled(true);
    timeline_->setValue(static_cast<int>(index));
    timeline_->setFrameLabel(static_cast<int>(index + 1), static_cast<int>(n));
    const double fps = photosPanel_->playbackFps();
    const int64_t currentUs =
        fps > 0.0 ? static_cast<int64_t>(static_cast<double>(index) * 1000000.0 / fps) : 0;
    const int64_t totalUs =
        fps > 0.0 ? static_cast<int64_t>(static_cast<double>(n) * 1000000.0 / fps) : 0;
    timeline_->setTimeLabel(QStringLiteral("%1 / %2")
                                .arg(formatTime(currentUs), formatTime(totalUs)));
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
    return photosPanel_->isBusy() || worker_ != nullptr || videoExportWorker_ != nullptr;
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

    trackerCombo_->setCurrentIndex(std::clamp(v.tracker, 0, 2));
    smoothSpin_->setValue(v.smoothingAlpha);
    borderCombo_->setCurrentIndex(std::clamp(v.borderMode, 0, 1));

    // La ROI y el inicio del análisis; los offsets del seguimiento no se
    // guardan (se recalculan con "Calcular automáticamente").
    roi_ = v.hasRoi ? QRect(v.roiX, v.roiY, v.roiW, v.roiH) : QRect();
    view_->setRoiEnabled(true);
    if (!roi_.isEmpty())
        view_->setRoi(roi_);
    else
        view_->clearRoi();
    view_->clearCircle();
    resultView_->clearCircle();
    offsets_.clear();
    trackSamples_.clear();
    hasSeed_ = false;
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
        tr("Vídeo restaurado: pulsa \"Calcular automáticamente\" para recalcular "
           "el análisis"), 6000);
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
    trackSamples_.clear();
    currentFrameImage_ = cv::Mat();
    hasSeed_ = false;
    previewEnabled_ = false;
    previewAction_->setChecked(false);
    view_->setFrame(cv::Mat());
    resultView_->setFrame(cv::Mat());
    view_->clearRoi();
    view_->clearCircle();
    resultView_->clearCircle();
    timeline_->setEnabled(false);
    timeline_->setValue(0);
    timeline_->setFrameLabel(-1, -1);
    timeline_->setTimeLabel("00:00:00.000");
    updateTransportUi();
    updateStabilizationUi();
    updatePanelMode();
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

    QSettings settings;
    settings.setValue("UI/geometry", saveGeometry());
    settings.setValue("UI/state", saveState());

    event->accept();
}