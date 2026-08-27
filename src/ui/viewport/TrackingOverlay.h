#pragma once

#include <QWidget>
#include <QPointF>
#include <QRect>
#include <QRectF>

class QPaintEvent;
class QMouseEvent;
class QWheelEvent;

// Overlay painted on top of VideoView showing tracking information:
// ROI rectangle, circle, detected/target centers, offset arrow, status badge.
class TrackingOverlay : public QWidget
{
    Q_OBJECT
public:
    enum class Status { None, Valid, Predicted, Lost, Uncertain };

    explicit TrackingOverlay(QWidget* parent = nullptr);

    // Image coordinate system setters (all in original image pixels).
    void setRoi(const QRectF& roi);
    void clearRoi();
    void setRoiSelecting(const QRectF& sel);

    void setCircle(const QPointF& center, float radius);
    void clearCircle();

    void setDetectedCenter(const QPointF& center);
    void setTargetCenter(const QPointF& target);
    void clearCenters();

    void setOffset(const QPointF& offset); // dx, dy from detected to target
    void clearOffset();

    void setStatus(Status s);
    void setTrackingText(const QString& text);

    // Must be called whenever the underlying image size changes so coordinate
    // transforms remain correct.
    void setImageRect(const QRectF& imageRect, int imageW, int imageH);

signals:
    void roiSelected(const QRect& imageRect);

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    QPointF toWidget(const QPointF& imgPt) const;
    QRectF toWidget(const QRectF& imgRect) const;

    QRectF roi_;
    bool roiVisible_ = false;
    QRectF roiSelecting_;

    QPointF circleCenter_;
    float circleRadius_ = 0;
    bool circleVisible_ = false;

    QPointF detectedCenter_;
    bool detectedVisible_ = false;
    QPointF targetCenter_;
    bool targetVisible_ = false;

    QPointF offset_;
    bool offsetVisible_ = false;

    Status status_ = Status::None;
    QString trackingText_;

    QRectF imageRect_;
    int imageW_ = 0;
    int imageH_ = 0;
};
