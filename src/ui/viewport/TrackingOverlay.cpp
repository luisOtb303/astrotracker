#include "ui/viewport/TrackingOverlay.h"

#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QFontMetrics>
#include <cmath>

TrackingOverlay::TrackingOverlay(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
}

void TrackingOverlay::setRoi(const QRectF& roi)
{
    roi_ = roi;
    roiVisible_ = !roi_.isNull() && !roi_.isEmpty();
    update();
}

void TrackingOverlay::clearRoi()
{
    roi_ = QRectF();
    roiVisible_ = false;
    update();
}

void TrackingOverlay::setRoiSelecting(const QRectF& sel)
{
    roiSelecting_ = sel;
    update();
}

void TrackingOverlay::setCircle(const QPointF& center, float radius)
{
    circleCenter_ = center;
    circleRadius_ = radius;
    circleVisible_ = radius > 0;
    update();
}

void TrackingOverlay::clearCircle()
{
    circleCenter_ = QPointF();
    circleRadius_ = 0;
    circleVisible_ = false;
    update();
}

void TrackingOverlay::setDetectedCenter(const QPointF& center)
{
    detectedCenter_ = center;
    detectedVisible_ = true;
    update();
}

void TrackingOverlay::setTargetCenter(const QPointF& target)
{
    targetCenter_ = target;
    targetVisible_ = true;
    update();
}

void TrackingOverlay::clearCenters()
{
    detectedVisible_ = false;
    targetVisible_ = false;
    update();
}

void TrackingOverlay::setOffset(const QPointF& offset)
{
    offset_ = offset;
    offsetVisible_ = true;
    update();
}

void TrackingOverlay::clearOffset()
{
    offsetVisible_ = false;
    update();
}

void TrackingOverlay::setStatus(Status s)
{
    status_ = s;
    update();
}

void TrackingOverlay::setTrackingText(const QString& text)
{
    trackingText_ = text;
    update();
}

void TrackingOverlay::setImageRect(const QRectF& imageRect, int imageW, int imageH)
{
    imageRect_ = imageRect;
    imageW_ = imageW;
    imageH_ = imageH;
}

QPointF TrackingOverlay::toWidget(const QPointF& imgPt) const
{
    if (imageRect_.isEmpty() || imageW_ <= 0 || imageH_ <= 0)
        return imgPt;
    const double sx = imageRect_.width() / imageW_;
    const double sy = imageRect_.height() / imageH_;
    return QPointF(imageRect_.left() + imgPt.x() * sx,
                   imageRect_.top() + imgPt.y() * sy);
}

QRectF TrackingOverlay::toWidget(const QRectF& imgRect) const
{
    if (imgRect.isNull() || imageRect_.isEmpty() || imageW_ <= 0 || imageH_ <= 0)
        return imgRect;
    const QPointF tl = toWidget(imgRect.topLeft());
    const double sx = imageRect_.width() / imageW_;
    const double sy = imageRect_.height() / imageH_;
    return QRectF(tl, QSizeF(imgRect.width() * sx, imgRect.height() * sy));
}

void TrackingOverlay::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // --- ROI ---
    if (roiVisible_) {
        const QRectF wr = toWidget(roi_);
        p.setPen(QPen(QColor(255, 200, 0), 2)); // gold
        p.setBrush(Qt::NoBrush);
        p.drawRect(wr);
    }

    // --- ROI selecting (in-progress drag) ---
    if (!roiSelecting_.isNull()) {
        p.setPen(QPen(QColor(0, 210, 0), 2, Qt::DashLine)); // green dashed
        p.setBrush(QColor(0, 210, 0, 30));
        p.drawRect(toWidget(roiSelecting_));
    }

    // --- Circle ---
    if (circleVisible_) {
        const QPointF cw = toWidget(circleCenter_);
        const double sx = imageRect_.width() / std::max(1, imageW_);
        const double rw = circleRadius_ * sx;

        QPen pen(QColor(0, 210, 0), 2); // green
        pen.setCosmetic(true);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(cw, rw, rw);
    }

    // --- Detected center (green crosshair) ---
    if (detectedVisible_) {
        const QPointF dw = toWidget(detectedCenter_);
        p.setPen(QPen(QColor(0, 210, 0), 1));
        p.drawLine(dw + QPointF(-8, 0), dw + QPointF(8, 0));
        p.drawLine(dw + QPointF(0, -8), dw + QPointF(0, 8));
    }

    // --- Target center (red crosshair) ---
    if (targetVisible_) {
        const QPointF tw = toWidget(targetCenter_);
        p.setPen(QPen(QColor(255, 68, 68), 1)); // red
        p.drawLine(tw + QPointF(-8, 0), tw + QPointF(8, 0));
        p.drawLine(tw + QPointF(0, -8), tw + QPointF(0, 8));
    }

    // --- Offset arrow (gold, from detected toward target) ---
    if (offsetVisible_ && detectedVisible_) {
        const QPointF dw = toWidget(detectedCenter_);
        const double sx = imageRect_.width() / std::max(1, imageW_);
        const double sy = imageRect_.height() / std::max(1, imageH_);
        const QPointF arrowEnd = dw + QPointF(offset_.x() * sx, offset_.y() * sy);
        p.setPen(QPen(QColor(255, 200, 0), 2)); // gold
        p.drawLine(dw, arrowEnd);
        // Arrowhead
        const double angle = std::atan2(arrowEnd.y() - dw.y(), arrowEnd.x() - dw.x());
        const double headLen = 8;
        p.drawLine(arrowEnd,
                   arrowEnd + QPointF(-headLen * std::cos(angle - 0.4),
                                      -headLen * std::sin(angle - 0.4)));
        p.drawLine(arrowEnd,
                   arrowEnd + QPointF(-headLen * std::cos(angle + 0.4),
                                      -headLen * std::sin(angle + 0.4)));
    }

    // --- Status badge (top-right corner) ---
    if (status_ != Status::None) {
        QString label;
        QColor bgColor;
        switch (status_) {
        case Status::Valid:
            label = tr("VALID");
            bgColor = QColor(15, 123, 15); // green
            break;
        case Status::Predicted:
            label = tr("PREDICTED");
            bgColor = QColor(157, 93, 0); // orange
            break;
        case Status::Lost:
            label = tr("LOST");
            bgColor = QColor(196, 43, 28); // red
            break;
        case Status::Uncertain:
            label = tr("UNCERTAIN");
            bgColor = QColor(128, 128, 128); // gray
            break;
        default:
            break;
        }

        if (!label.isEmpty()) {
            QFont f = p.font();
            f.setPointSize(9);
            f.setBold(true);
            p.setFont(f);
            const QFontMetrics fm(f);
            const int pad = 6;
            const int tw = fm.horizontalAdvance(label) + pad * 2;
            const int th = fm.height() + pad * 2;
            const QRect badgeRect(width() - tw - 8, 8, tw, th);
            p.setPen(Qt::NoPen);
            p.setBrush(bgColor);
            p.drawRoundedRect(badgeRect, 4, 4);
            p.setPen(Qt::white);
            p.drawText(badgeRect, Qt::AlignCenter, label);
        }
    }

    // --- Tracking text (below badge) ---
    if (!trackingText_.isEmpty()) {
        QFont f = p.font();
        f.setPointSize(8);
        p.setFont(f);
        const QFontMetrics fm(f);
        const int tw = fm.horizontalAdvance(trackingText_);
        const QRect textRect(width() - tw - 12, 36, tw + 8, fm.height() + 4);
        p.setPen(QColor(200, 200, 200));
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignRight, trackingText_);
    }
}
