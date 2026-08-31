#pragma once

#include <QWidget>
#include <QPointF>
#include <opencv2/core.hpp>

// Visor de vídeo/fotos: pinta el frame actual y permite seleccionar una ROI
// (rectángulo) con el ratón, o dibujar/editar un círculo (centro+radio) cuando
// el modo círculo está activo. Las coordenadas se expresan en píxeles de imagen.
class VideoView : public QWidget
{
    Q_OBJECT

public:
    explicit VideoView(QWidget* parent = nullptr);

    void setFrame(const cv::Mat& frame);
    void setRoiEnabled(bool enabled);
    void setRoi(const QRect& roi);
    void clearRoi();

    // Muestra un indicador "Abriendo foto…" mientras se decodifica el frame en
    // segundo plano.
    void setLoading(bool loading);

    // Zoom: 1.0 = ajustar al tamaño del visor; mayor = factor sobre ese ajuste.
    void setZoomFit();
    void setZoomPercent(int percent);
    void zoomIn();
    void zoomOut();
    bool isZoomed() const { return zoom_ != 1.0; }

    // Modo círculo: arrastrar pinta desde el centro, mover arrastra el centro
    // y el borde cambia el radio.
    void setCircleEnabled(bool enabled);
    void setCircle(const QPointF& center, double radius, bool predicted = false);
    void clearCircle();
    bool hasCircle() const { return circleRadius_ > 0.0; }
    QPointF circleCenter() const { return circleCenter_; }
    double circleRadius() const { return circleRadius_; }

signals:
    void roiSelected(const QRect& rect);
    void circleSelected(const QPointF& center, double radius);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    enum class CircleEdit
    {
        None,
        New,    // se pinta desde el centro hacia el radio
        Move,   // arrastrar el centro
        Resize, // arrastrar el borde para cambiar el radio
    };

    QRect imageRect() const;
    QRect toWidget(const QRect& imageRect) const;
    QPointF toWidget(const QPointF& imagePoint) const;
    QPointF toImage(const QPointF& widgetPoint) const;
    void drawLoading(QPainter& painter) const;

    QImage image_;
    QPoint selStart_;
    QRect selection_;
    QRect roi_;
    bool selecting_ = false;
    bool roiEnabled_ = true;

    bool circleEnabled_ = false;
    bool loading_ = false;
    QPointF circleCenter_;   // píxeles de imagen
    double circleRadius_ = 0.0;
    bool circlePredicted_ = false;
    CircleEdit circleEdit_ = CircleEdit::None;
    QPointF dragStart_;      // imagen
    QPointF dragOrigin_;     // imagen

    // Zoom/pan.
    double zoom_ = 1.0;      // 1.0 = ajustar al visor; mayor = escala sobre el ajuste
    QPointF offset_;         // desplazamiento en píxeles de widget
    bool panning_ = false;
    QPointF panStart_;
    QPointF offsetOrigin_;
};