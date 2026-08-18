#pragma once

#include "tracking/ITracker.h"

// TemplateTracker: busca el template inicial dentro de una ventana de búsqueda
// alrededor de la última posición usando matchTemplate (TM_CCOEFF_NORMED).
// Adecuado para el Sol/Luna: objeto brillante de forma casi constante.
class TemplateTracker final : public ITracker
{
public:
    // searchFactor: la ventana de búsqueda es el template escalado por este factor.
    explicit TemplateTracker(float searchFactor = 2.5f, float minConfidence = 0.25f);

    bool init(const cv::Mat& frame, const cv::Rect2f& roi) override;
    TrackResult track(const cv::Mat& frame) override;

private:
    float searchFactor_;
    float minConfidence_;
    cv::Mat tmpl_;
    cv::Rect lastRect_;
    bool initialized_ = false;
};