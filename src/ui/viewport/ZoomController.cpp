#include "ui/viewport/ZoomController.h"

#include <QWidget>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QRectF>
#include <algorithm>
#include <cmath>

ZoomController::ZoomController(QWidget* target, QObject* parent)
    : QObject(parent), target_(target)
{
}

void ZoomController::zoomIn()
{
    setZoom(zoomFactor_ * ZoomStep);
}

void ZoomController::zoomOut()
{
    setZoom(zoomFactor_ / ZoomStep);
}

void ZoomController::zoomFit()
{
    setZoom(1.0);
    panOffset_ = QPointF();
    target_->update();
    emit panChanged(panOffset_);
}

void ZoomController::zoomActual()
{
    setZoom(1.0);
    panOffset_ = QPointF();
    target_->update();
    emit panChanged(panOffset_);
}

void ZoomController::zoomTo(double factor, const QPointF& center)
{
    setZoom(factor);
    target_->update();
}

void ZoomController::handleWheel(QWheelEvent* e)
{
    const double oldZoom = zoomFactor_;
    if (e->angleDelta().y() > 0)
        setZoom(zoomFactor_ * ZoomStep);
    else
        setZoom(zoomFactor_ / ZoomStep);

    // Zoom centered on cursor position
    if (zoomFactor_ != oldZoom) {
        const QPointF cursor = e->position();
        const double ratio = zoomFactor_ / oldZoom;
        panOffset_ = cursor - ratio * (cursor - panOffset_);
        target_->update();
        emit panChanged(panOffset_);
    }
    e->accept();
}

void ZoomController::handleMousePress(QMouseEvent* e)
{
    if (e->button() == Qt::MiddleButton) {
        panning_ = true;
        panStart_ = e->pos();
        target_->setCursor(Qt::ClosedHandCursor);
        e->accept();
    }
}

void ZoomController::handleMouseMove(QMouseEvent* e)
{
    if (panning_) {
        const QPointF delta = e->pos() - panStart_;
        panStart_ = e->pos();
        panOffset_ += delta;
        target_->update();
        emit panChanged(panOffset_);
        e->accept();
    }
}

void ZoomController::handleMouseRelease(QMouseEvent* e)
{
    if (e->button() == Qt::MiddleButton && panning_) {
        panning_ = false;
        target_->unsetCursor();
        e->accept();
    }
}

QRectF ZoomController::effectiveImageRect(const QRectF& originalImageRect) const
{
    if (originalImageRect.isEmpty())
        return {};
    const QPointF center = originalImageRect.center();
    const double w = originalImageRect.width() * zoomFactor_;
    const double h = originalImageRect.height() * zoomFactor_;
    const QPointF topLeft = QPointF(center.x() - w / 2.0, center.y() - h / 2.0) + panOffset_;
    return QRectF(topLeft, QSizeF(w, h));
}

void ZoomController::setZoom(double factor)
{
    factor = std::clamp(factor, MinZoom, MaxZoom);
    if (std::abs(factor - zoomFactor_) < 1e-6)
        return;
    zoomFactor_ = factor;
    emit zoomChanged(zoomFactor_);
}
