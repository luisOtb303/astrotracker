#include "ui/VideoView.h"

#include <QPainter>
#include <QMouseEvent>
#include <opencv2/imgproc.hpp>
#include <cmath>

VideoView::VideoView(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(320, 240);
    setMouseTracking(true);
}

void VideoView::setFrame(const cv::Mat& frame)
{
    if (frame.empty()) {
        image_ = QImage();
        update();
        return;
    }
    cv::Mat rgb;
    cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
    image_ = QImage(rgb.data, rgb.cols, rgb.rows,
                    static_cast<int>(rgb.step), QImage::Format_RGB888)
                 .copy();
    update();
}

QRect VideoView::imageRect() const
{
    if (image_.isNull())
        return QRect();
    const double scale = std::min(width() / static_cast<double>(image_.width()),
                                  height() / static_cast<double>(image_.height()));
    const int w = static_cast<int>(image_.width() * scale);
    const int h = static_cast<int>(image_.height() * scale);
    return QRect((width() - w) / 2, (height() - h) / 2, w, h);
}

void VideoView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(20, 20, 20));

    if (image_.isNull()) {
        p.setPen(QColor(140, 140, 140));
        p.drawText(rect(), Qt::AlignCenter, tr("Abrir un vídeo para empezar"));
        return;
    }

    p.drawImage(imageRect(), image_);

    if (!selection_.isEmpty()) {
        p.setPen(QPen(QColor(0, 210, 0), 2));
        p.drawRect(selection_);
    }
}

void VideoView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !image_.isNull()) {
        selStart_ = event->pos();
        selection_ = QRect(selStart_, QSize());
        selecting_ = true;
    }
}

void VideoView::mouseMoveEvent(QMouseEvent* event)
{
    if (selecting_) {
        selection_ = QRect(selStart_, event->pos()).normalized();
        update();
    }
}

void VideoView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !selecting_)
        return;
    selecting_ = false;
    selection_ = QRect(selStart_, event->pos()).normalized();
    update();

    if (selection_.isEmpty() || image_.isNull())
        return;

    const QRect ir = imageRect();
    const double sx = image_.width() / static_cast<double>(ir.width());
    const double sy = image_.height() / static_cast<double>(ir.height());

    QRect roi(qRound((selection_.left() - ir.left()) * sx),
              qRound((selection_.top() - ir.top()) * sy),
              qRound(selection_.width() * sx),
              qRound(selection_.height() * sy));
    roi = roi.intersected(QRect(0, 0, image_.width(), image_.height()));

    if (!roi.isEmpty())
        emit roiSelected(roi);
}