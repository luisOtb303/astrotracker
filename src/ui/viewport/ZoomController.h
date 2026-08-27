#pragma once

#include <QObject>
#include <QPointF>
#include <QRectF>

class QWidget;

// Manages zoom level and panning state for a viewport widget.
// Attach to a QWidget and call the handlers from its event methods.
class ZoomController : public QObject
{
    Q_OBJECT
public:
    explicit ZoomController(QWidget* target, QObject* parent = nullptr);

    void zoomIn();
    void zoomOut();
    void zoomFit();
    void zoomActual();
    void zoomTo(double factor, const QPointF& center = QPointF());

    double zoomFactor() const { return zoomFactor_; }
    bool isPanning() const { return panning_; }
    QPointF panOffset() const { return panOffset_; }

    // Call from the target widget's wheelEvent.
    void handleWheel(class QWheelEvent* e);
    // Call from the target widget's mousePressEvent (for middle button pan).
    void handleMousePress(class QMouseEvent* e);
    // Call from the target widget's mouseMoveEvent.
    void handleMouseMove(class QMouseEvent* e);
    // Call from the target widget's mouseReleaseEvent.
    void handleMouseRelease(class QMouseEvent* e);

    // Returns the effective image rect after applying zoom and pan.
    QRectF effectiveImageRect(const QRectF& originalImageRect) const;

signals:
    void zoomChanged(double factor);
    void panChanged(const QPointF& offset);

private:
    void setZoom(double factor);

    QWidget* target_;
    double zoomFactor_ = 1.0;
    QPointF panOffset_;
    bool panning_ = false;
    QPointF panStart_;

    static constexpr double MinZoom = 0.1;
    static constexpr double MaxZoom = 20.0;
    static constexpr double ZoomStep = 1.25;
};
