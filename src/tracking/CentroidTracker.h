#pragma once

#include "tracking/ITracker.h"

// CentroidTracker: segmente el objeto brillante (umbral OTSU dentro de la
// ventana de búsqueda) y sigue su centroide por el componente conexo más grande.
// Adecuado para el Sol/Luna sobre fondo oscuro.
class CentroidTracker final : public ITracker
{
public:
    // searchFactor: ventana de búsqueda = ROI * factor.
    // minAreaFraction: fracción mínima del área de la ROI para considerar válido.
    explicit CentroidTracker(float searchFactor = 2.5f, float minAreaFraction = 0.15f);

    bool init(const cv::Mat& frame, const cv::Rect2f& roi) override;
    TrackResult track(const cv::Mat& frame) override;

private:
    float searchFactor_;
    float minAreaFraction_;
    cv::Rect lastRect_;
    cv::Rect roi_; // tamaño de referencia del objeto
    bool initialized_ = false;
};