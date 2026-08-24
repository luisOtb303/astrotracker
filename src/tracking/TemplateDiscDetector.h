#pragma once

#include "tracking/IDiscDetector.h"

#include <opencv2/core.hpp>

// Detector por plantilla: el parche del último disco confirmado se busca con
// matchTemplate (TM_SQDIFF_NORMED) primero en la ventana de búsqueda y, si el
// objeto ha saltado lejos, en todo el frame. SQDIFF no es invariante a la
// exposición, así que trabaja sobre la copia normalizada del contexto.
class TemplateDiscDetector final : public IDiscDetector
{
public:
    explicit TemplateDiscDetector(const DiscTrackerParams& params);

    DiscMethod method() const override { return DiscMethod::Template; }

    std::vector<DiscDetection> detect(const DetectorContext& ctx) override;

    // Refresca la plantilla con el parche del disco confirmado.
    void onConfirmed(const DetectorContext& ctx,
                     const cv::Point2f& confirmedCenter) override;

    void onMissed() override {}

    bool hasTemplate() const { return !template_.empty(); }

private:
    DiscTrackerParams p_;
    cv::Mat template_;
};
