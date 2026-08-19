#include "ui/PhotoPanel.h"

#include "ui/VideoView.h"

#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPixmap>
#include <QProgressDialog>
#include <QSlider>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <opencv2/imgproc.hpp>

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
    prevAction_ = tb->addAction(tr("Anterior"), this, &PhotoPanel::showPrev);
    nextAction_ = tb->addAction(tr("Siguiente"), this, &PhotoPanel::showNext);
    tb->addSeparator();
    analyzeAction_ = tb->addAction(tr("Seguir secuencia"));
    analyzeAction_->setEnabled(false);
    analyzeAction_->setToolTip(tr("Disponible en la próxima etapa (seguimiento por círculo)"));
    exportAction_ = tb->addAction(tr("Exportar centradas..."));
    exportAction_->setEnabled(false);
    exportAction_->setToolTip(tr("Disponible en la próxima etapa (centrado y exportación)"));
    root->addWidget(tb);

    auto* viewers = new QHBoxLayout();

    auto* sourceBox = new QVBoxLayout();
    auto* srcTitle = new QLabel(tr("Original"), this);
    srcTitle->setAlignment(Qt::AlignCenter);
    sourceBox->addWidget(srcTitle);
    view_ = new VideoView(this);
    view_->setRoiEnabled(true);
    sourceBox->addWidget(view_, 1);

    auto* resultBox = new QVBoxLayout();
    auto* resTitle = new QLabel(tr("Centrado"), this);
    resTitle->setAlignment(Qt::AlignCenter);
    resultBox->addWidget(resTitle);
    resultView_ = new VideoView(this);
    resultView_->setRoiEnabled(false);
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

    prevAction_->setEnabled(false);
    nextAction_->setEnabled(false);
}

void PhotoPanel::openImagesDialog()
{
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, tr("Seleccionar fotos (secuencia)"), QString(),
        tr("Imágenes (*.jpg *.jpeg *.png *.tif *.tiff *.bmp);;Todos los archivos (*.*)"));
    if (paths.isEmpty())
        return;
    openPaths(paths);
}

void PhotoPanel::openFolderDialog()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Abrir carpeta de fotos"));
    if (dir.isEmpty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = reader_.openFolder(dir.toStdString());
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::warning(this, tr("AstroTracker"),
                             tr("La carpeta no contiene imágenes soportadas:\n%1").arg(dir));
        return;
    }
    reloadSequence();
}

void PhotoPanel::openPaths(const QStringList& paths)
{
    std::vector<std::string> v;
    v.reserve(static_cast<size_t>(paths.size()));
    for (const QString& p : paths)
        v.push_back(p.toStdString());

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = reader_.open(v);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::warning(this, tr("AstroTracker"),
                             tr("Ninguno de los archivos seleccionados es una imagen soportada."));
        return;
    }
    reloadSequence();
}

void PhotoPanel::clearSession()
{
    reader_.close();
    current_ = 0;
    filmstrip_->clear();
    view_->setFrame(cv::Mat());
    resultView_->setFrame(cv::Mat());
    slider_->setEnabled(false);
    prevAction_->setEnabled(false);
    nextAction_->setEnabled(false);
    indexLabel_->setText(tr("Foto: - / -"));
    infoLabel_->setText(tr("Abrir una carpeta o seleccionar fotos para empezar"));
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

    showCurrent();
    updateNavUi();
}

void PhotoPanel::buildFilmstrip()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QProgressDialog progress(tr("Generando miniaturas..."), QString(), 0,
                             static_cast<int>(reader_.count()), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(400);
    progress.setCancelButton(nullptr);

    for (int64_t i = 0; i < reader_.count(); ++i) {
        cv::Mat thumb;
        if (!reader_.readAt(i, thumb, thumbMaxDim_))
            continue;
        auto* item = new QListWidgetItem(QIcon(matToPixmap(thumb)), QString());
        item->setToolTip(QString::fromStdString(reader_.fileName(i)));
        item->setData(Qt::UserRole, static_cast<qlonglong>(i));
        filmstrip_->addItem(item);
        progress.setValue(static_cast<int>(i));
        QApplication::processEvents();
    }
    progress.setValue(static_cast<int>(reader_.count()));
    QApplication::restoreOverrideCursor();
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
    resultView_->setFrame(frame);
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
    infoLabel_->setText(QString::fromStdString(reader_.fileName(current_)));
}

QPixmap PhotoPanel::toPixmap(const cv::Mat& bgr)
{
    return matToPixmap(bgr);
}