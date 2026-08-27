#include "ui/timeline/FilmstripWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace {

QPixmap matToPixmap(const cv::Mat& bgr)
{
    if (bgr.empty()) return {};
    cv::Mat rgb;
    cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
    QImage img(rgb.data, rgb.cols, rgb.rows,
               static_cast<int>(rgb.step), QImage::Format_RGB888);
    return QPixmap::fromImage(img.copy());
}

} // namespace

// --- ThumbnailLoader ---

ThumbnailLoader::ThumbnailLoader(const std::vector<std::string>& paths, int maxDim,
                                 FilmstripWidget* target)
    : QThread(target), paths_(paths), maxDim_(maxDim), target_(target)
{
}

void ThumbnailLoader::run()
{
    for (int i = 0; i < static_cast<int>(paths_.size()); ++i) {
        cv::Mat thumb;
        cv::Mat img = cv::imread(paths_[static_cast<size_t>(i)], cv::IMREAD_COLOR);
        if (!img.empty()) {
            const int longest = std::max(img.cols, img.rows);
            if (longest > maxDim_) {
                const double scale = static_cast<double>(maxDim_) / longest;
                cv::resize(img, img, cv::Size(), scale, scale, cv::INTER_AREA);
            }
            thumb = img;
        }
        if (!thumb.empty())
            emit thumbnailReady(i, matToPixmap(thumb));

        if (isInterruptionRequested())
            return;
    }
    emit allLoaded();
}

// --- FilmstripWidget ---

FilmstripWidget::FilmstripWidget(QWidget* parent)
    : QListWidget(parent)
{
    setViewMode(QListView::IconMode);
    setMovement(QListView::Static);
    setResizeMode(QListView::Adjust);
    setWrapping(false);
    setUniformItemSizes(false);
    setIconSize(QSize(120, 80));
    setGridSize(QSize(140, 110));
    setSpacing(4);
    setFlow(QListView::LeftToRight);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setMinimumHeight(135);
}

void FilmstripWidget::loadThumbnails(const std::vector<std::string>& paths, int maxDim)
{
    clearAll();
    if (paths.empty()) return;

    for (int i = 0; i < static_cast<int>(paths.size()); ++i) {
        auto* item = new QListWidgetItem(this);
        item->setData(Qt::UserRole, static_cast<qlonglong>(i));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
    }

    startBackgroundLoader(paths, maxDim);
}

void FilmstripWidget::clearAll()
{
    if (loading_) {
        auto* loader = findChild<ThumbnailLoader*>();
        if (loader) {
            loader->requestInterruption();
            loader->wait(2000);
        }
    }
    clear();
    thumbnails_.clear();
    badges_.clear();
    badgeLabels_.clear();
    loading_ = false;
    currentFrame_ = -1;
}

void FilmstripWidget::setBadge(int index, BadgeStatus status, const QString& label)
{
    badges_[index] = status;
    badgeLabels_[index] = label;
    if (index < count()) {
        auto* item = this->item(index);
        QColor color;
        switch (status) {
        case BadgeStatus::Valid:     color = QColor(46, 139, 87);  break; // green
        case BadgeStatus::Predicted: color = QColor(255, 165, 0);  break; // orange
        case BadgeStatus::Lost:      color = QColor(192, 64, 64);  break; // red
        case BadgeStatus::Locked:    color = QColor(30, 144, 255); break; // blue
        case BadgeStatus::None:      color = QColor(180, 180, 180); break;
        }
        item->setText(label);
        item->setForeground(QBrush(color));
    }
}

void FilmstripWidget::setExportState(int index, bool checked)
{
    if (index < count()) {
        auto* item = this->item(index);
        item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    }
}

bool FilmstripWidget::exportState(int index) const
{
    if (index < count())
        return item(index)->checkState() == Qt::Checked;
    return true;
}

void FilmstripWidget::setCurrentFrame(int index)
{
    currentFrame_ = index;
    if (index >= 0 && index < count())
        setCurrentRow(index);
}

void FilmstripWidget::mouseReleaseEvent(QMouseEvent* event)
{
    QListWidget::mouseReleaseEvent(event);
    auto* item = itemAt(event->pos());
    if (!item) return;

    // Toggle checkbox if click is in the icon region (left side).
    if (event->pos().x() < iconSize().width() + 8) {
        const bool checked = item->checkState() == Qt::Checked;
        item->setCheckState(checked ? Qt::Unchecked : Qt::Checked);
        const int idx = item->data(Qt::UserRole).toInt();
        emit exportToggled(idx, !checked);
    }

    const int idx = item->data(Qt::UserRole).toInt();
    emit frameActivated(idx);
}

void FilmstripWidget::onThumbnailReady(int index, QPixmap pixmap)
{
    thumbnails_[index] = pixmap;
    if (index < count()) {
        auto* item = this->item(index);
        // Overlay badge if exists
        if (badges_.contains(index)) {
            QPainter p(&pixmap);
            // ... badge rendering handled by setBadge text color
            Q_UNUSED(p);
        }
        item->setIcon(QIcon(pixmap));
    }
}

void FilmstripWidget::onAllThumbnailsLoaded()
{
    loading_ = false;
}

void FilmstripWidget::startBackgroundLoader(const std::vector<std::string>& paths, int maxDim)
{
    loading_ = true;
    auto* loader = new ThumbnailLoader(paths, maxDim, this);
    connect(loader, &ThumbnailLoader::thumbnailReady, this, &FilmstripWidget::onThumbnailReady);
    connect(loader, &ThumbnailLoader::allLoaded, this, &FilmstripWidget::onAllThumbnailsLoaded);
    connect(loader, &QThread::finished, loader, &QObject::deleteLater);
    loader->start();
}
