#pragma once

#include <QWidget>
#include <opencv2/core.hpp>

// Visor de vídeo: pinta el frame actual y permite seleccionar una ROI con el
// ratón. Las coordenadas de la ROI se emiten en píxeles de imagen.
class VideoView : public QWidget
{
    Q_OBJECT

public:
    explicit VideoView(QWidget* parent = nullptr);

    void setFrame(const cv::Mat& frame);
    void setRoiEnabled(bool enabled);
    void setRoi(const QRect& roi);
    void clearRoi();

signals:
    void roiSelected(const QRect& rect);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QRect imageRect() const;
    QRect toWidget(const QRect& imageRect) const;

    QImage image_;
    QPoint selStart_;
    QRect selection_;
    QRect roi_;
    bool selecting_ = false;
    bool roiEnabled_ = true;
};