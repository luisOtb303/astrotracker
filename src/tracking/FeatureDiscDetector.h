#pragma once

#include "tracking/DiscTrackerParams.h"
#include "tracking/IDiscDetector.h"

#include <opencv2/core.hpp>
#include <vector>

// Detector por features de superficie: puntos Shi-Tomasi sobre el frame
// anterior y flujo óptico Lucas-Kanade al frame actual; la mediana de los
// flujos (robusta a outliers) da el desplazamiento del disco y el ratio de
// inliers la confianza. Requiere textura (superficie lunar, manchas solares);
// no contribuye sin ella.
class FeatureDiscDetector final : public IDiscDetector
{
public:
    explicit FeatureDiscDetector(const DiscTrackerParams& params);

    DiscMethod method() const override { return DiscMethod::Features; }

    std::vector<DiscDetection> detect(const DetectorContext& ctx) override;

    // Captura puntos de interés alrededor del disco confirmado; a partir de
    // aquí el detector puede seguirlos en la foto siguiente.
    void onConfirmed(const DetectorContext& ctx,
                     const cv::Point2f& confirmedCenter) override;

    // Sin confirmación se pierde la referencia: reinicia el estado.
    void onMissed() override;

private:
    DiscTrackerParams p_;
    cv::Mat prevGray_;                    // frame anterior (gris suavizado)
    std::vector<cv::Point2f> prevPoints_; // puntos seguidos
    cv::Point2f prevCenter_{0.f, 0.f};    // centro del disco en ese frame
};
