#include "ui/VideoView.h"

#include <QPainter>
#include <QMouseEvent>
#include <opencv2/imgproc.hpp>
#include <algorithm>
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

void VideoView::setRoiEnabled(bool enabled)
{
    roiEnabled_ = enabled;
    if (!enabled) {
        selection_ = QRect();
        circleEdit_ = CircleEdit::None;
    }
    update();
}

void VideoView::setRoi(const QRect& roi)
{
    roi_ = roi;
    update();
}

void VideoView::clearRoi()
{
    roi_ = QRect();
    selection_ = QRect();
    update();
}

void VideoView::setCircleEnabled(bool enabled)
{
    circleEnabled_ = enabled;
    if (!enabled)
        circleEdit_ = CircleEdit::None;
    update();
}

void VideoView::setCircle(const QPointF& center, double radius, bool predicted)
{
    circleCenter_ = center;
    circleRadius_ = radius;
    circlePredicted_ = predicted;
    update();
}

void VideoView::clearCircle()
{
    circleCenter_ = QPointF();
    circleRadius_ = 0.0;
    circlePredicted_ = false;
    circleEdit_ = CircleEdit::None;
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

QRect VideoView::toWidget(const QRect& imgRect) const
{
    if (imgRect.isEmpty() || image_.isNull())
        return QRect();
    const QRect ir = imageRect();
    const double sx = ir.width() / static_cast<double>(image_.width());
    const double sy = ir.height() / static_cast<double>(image_.height());
    return QRect(qRound(ir.left() + imgRect.left() * sx),
                 qRound(ir.top() + imgRect.top() * sy),
                 qRound(imgRect.width() * sx),
                 qRound(imgRect.height() * sy));
}

QPointF VideoView::toWidget(const QPointF& imagePoint) const
{
    const QRect ir = imageRect();
    const double sx = ir.width() / static_cast<double>(image_.width());
    const double sy = ir.height() / static_cast<double>(image_.height());
    return QPointF(ir.left() + imagePoint.x() * sx,
                   ir.top() + imagePoint.y() * sy);
}

QPointF VideoView::toImage(const QPointF& widgetPoint) const
{
    const QRect ir = imageRect();
    if (ir.isEmpty() || image_.isNull())
        return QPointF();
    const double sx = image_.width() / static_cast<double>(ir.width());
    const double sy = image_.height() / static_cast<double>(ir.height());
    return QPointF((widgetPoint.x() - ir.left()) * sx,
                   (widgetPoint.y() - ir.top()) * sy);
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

    if (!roi_.isEmpty()) {
        p.setPen(QPen(QColor(255, 200, 0), 2));
        p.drawRect(toWidget(roi_));
    }

    if (!selection_.isEmpty()) {
        p.setPen(QPen(QColor(0, 210, 0), 2));
        p.drawRect(selection_);
    }

    if (hasCircle()) {
        const QPointF cw = toWidget(circleCenter_);
        const QRect ir = imageRect();
        const double sx = ir.width() / static_cast<double>(image_.width());
        const double rw = circleRadius_ * sx;

        QPen pen(QColor(0, 210, 0), 2);
        if (circlePredicted_)
            pen.setStyle(Qt::DashLine);
        pen.setCosmetic(true);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(cw, rw, rw);

        // Cruz en el centro.
        p.setPen(QPen(QColor(0, 210, 0), 1));
        p.drawLine(cw + QPointF(-6, 0), cw + QPointF(6, 0));
        p.drawLine(cw + QPointF(0, -6), cw + QPointF(0, 6));
    }
}

void VideoView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || image_.isNull())
        return;

    if (circleEnabled_) {
        const QPointF img = toImage(event->pos());
        const QPointF cw = toWidget(circleCenter_);
        const QPointF wp = event->pos();

        if (hasCircle()) {
            const double dCenter = std::hypot(wp.x() - cw.x(), wp.y() - cw.y());
            const QRect ir = imageRect();
            const double sx = ir.width() / static_cast<double>(image_.width());
            const double rw = circleRadius_ * sx;
            if (dCenter <= 12.0) {
                circleEdit_ = CircleEdit::Move;
                dragOrigin_ = circleCenter_; // centro original (imagen)
            } else if (std::abs(dCenter - rw) <= 8.0) {
                circleEdit_ = CircleEdit::Resize;
                // el centro no cambia al redimensionar
            } else {
                circleEdit_ = CircleEdit::New;
                circleCenter_ = img;
                circleRadius_ = 0.0;
            }
        } else {
            circleEdit_ = CircleEdit::New;
            circleCenter_ = img;
            circleRadius_ = 0.0;
        }
        dragStart_ = img;
        circlePredicted_ = false;
        return;
    }

    if (roiEnabled_) {
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
        return;
    }

    if (circleEdit_ != CircleEdit::None) {
        const QPointF img = toImage(event->pos());
        const QPointF delta = img - dragStart_;
        switch (circleEdit_) {
        case CircleEdit::Move:
            circleCenter_ = dragOrigin_ + delta;
            break;
        case CircleEdit::Resize:
            circleRadius_ = std::hypot(img.x() - circleCenter_.x(),
                                       img.y() - circleCenter_.y());
            break;
        case CircleEdit::New:
            circleRadius_ = std::hypot(img.x() - circleCenter_.x(),
                                       img.y() - circleCenter_.y());
            break;
        default:
            break;
        }
        update();
    }
}

void VideoView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;

    if (circleEdit_ != CircleEdit::None) {
        const CircleEdit done = circleEdit_;
        circleEdit_ = CircleEdit::None;
        circlePredicted_ = false;
        if (circleRadius_ > 0.0)
            emit circleSelected(circleCenter_, circleRadius_);
        else if (done == CircleEdit::Move)
            emit circleSelected(circleCenter_, circleRadius_);
        update();
        return;
    }

    if (!selecting_)
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