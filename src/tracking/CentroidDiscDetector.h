#pragma once

#include "tracking/DiscTrackerParams.h"
#include "tracking/IDiscDetector.h"

// Detector por centroide: umbraliza (Otsu interno de blobInfo) y devuelve el
// centroide del mayor blob brillante si es compatible con un disco lleno
// (redondeado y de área ~πR²). Es el método natural para planetas y discos
// completos; NO sirve en fases parciales (el centroide del creciente no es el
// centro del disco).
class CentroidDiscDetector final : public IDiscDetector
{
public:
    explicit CentroidDiscDetector(const DiscTrackerParams& params);

    DiscMethod method() const override { return DiscMethod::Centroid; }

    std::vector<DiscDetection> detect(const DetectorContext& ctx) override;

    void onConfirmed(const DetectorContext& ctx,
                     const cv::Point2f& confirmedCenter) override;

    void onMissed() override;
};
