#include "ui/MainWindow.h"

#include "ui/VideoView.h"
#include "video/IVideoReader.h"
#include "video/FFmpegVideoReader.h"

#include <QAction>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QSlider>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QTime>
#include <algorithm>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();
    timer_ = new QTimer(this);
    timer_->setInterval(33);
    connect(timer_, &QTimer::timeout, this, &MainWindow::onTimer);
    connect(view_, &VideoView::roiSelected, this, &MainWindow::onRoiSelected);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    setWindowTitle(tr("AstroTracker"));
    resize(1100, 720);

    QMenu* fileMenu = menuBar()->addMenu(tr("&Archivo"));
    QAction* openAction = fileMenu->addAction(tr("&Abrir vídeo..."), this,
                                              &MainWindow::openFile, QKeySequence::Open);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Salir"), this, &QWidget::close);

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

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(4, 4, 4, 4);

    view_ = new VideoView(central);
    layout->addWidget(view_, 1);

    slider_ = new QSlider(Qt::Horizontal, central);
    slider_->setEnabled(false);
    layout->addWidget(slider_);

    auto* bottom = new QHBoxLayout();
    frameLabel_ = new QLabel(tr("Frame: - / -"), central);
    timeLabel_ = new QLabel(tr("00:00:00.000"), central);
    timeLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    bottom->addWidget(frameLabel_, 1);
    bottom->addWidget(timeLabel_);
    layout->addLayout(bottom);

    setCentralWidget(central);
    statusBar()->showMessage(tr("Abrir un vídeo para empezar"));

    connect(slider_, &QSlider::valueChanged, this, [this](int ms) {
        if (!reader_ || ms == currentUs_ / 1000)
            return;
        reader_->seekToUs(static_cast<int64_t>(ms) * 1000);
        showCurrentFrame();
    });
}

void MainWindow::openFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Abrir vídeo"), QString(),
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

    reader_ = std::move(reader);
    totalUs_ = reader_->durationUs();
    totalFrames_ = reader_->frameCount();
    if (reader_->fps() > 0.0)
        stepUs_ = static_cast<int64_t>(1000000.0 / reader_->fps());
    currentUs_ = 0;
    roi_ = QRect();

    slider_->setRange(0, static_cast<int>(totalUs_ / 1000));
    slider_->setValue(0);
    slider_->setEnabled(true);
    playAction_->setEnabled(true);

    showCurrentFrame();
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
    slider_->setValue(static_cast<int>(currentUs_ / 1000));
    frameLabel_->setText(tr("Frame: %1 / %2").arg(frame.index).arg(totalFrames_));
    timeLabel_->setText(formatTime(currentUs_));
}

void MainWindow::onRoiSelected(const QRect& rect)
{
    roi_ = rect;
    statusBar()->showMessage(tr("ROI seleccionada: %1x%2 en (%3,%4)")
                                 .arg(rect.width())
                                 .arg(rect.height())
                                 .arg(rect.x())
                                 .arg(rect.y()),
                             5000);
}

void MainWindow::updateTransportUi()
{
    // no-op por ahora
}

QString MainWindow::formatTime(int64_t us) const
{
    return QTime(0, 0).addMSecs(static_cast<int>(us / 1000)).toString("hh:mm:ss.zzz");
}