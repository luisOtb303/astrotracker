#pragma once

#include <opencv2/core.hpp>
#include <vector>

// Ajusta un círculo de radio fijo a partir de puntos del limbo del disco.
// El radio se mantiene fijo (decisión de diseño); solo se estima el centro.
struct CircleEstimate {
    bool ok = false;
    cv::Point2f center{0.f, 0.f};
    float radius = 0.f;
    // Fracción de puntos que quedan sobre el círculo (inliers / total).
    float inlierRatio = 0.f;
};

class CircleEstimator
{
public:
    // Ajusta un círculo de radio fijo `radius`, arrancando cerca de
    // `priorCenter`. Los puntos que no quedan sobre la circunferencia se
    // descartan como outliers. Robusto ante arcos parciales (creciente,
    // eclipse parcial) gracias al prior y al radio fijo.
    static CircleEstimate fit(const std::vector<cv::Point2f>& points,
                              const cv::Point2f& priorCenter, float radius,
                              float tolerance);
};