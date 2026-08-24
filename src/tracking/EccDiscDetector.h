#pragma once

#include "tracking/DiscTrackerParams.h"
#include "tracking/IDiscDetector.h"

#include <opencv2/core.hpp>

// Detector ECC (findTransformECC, traslación): alineamiento fino subpíxel
// entre el parche de referencia del último disco confirmado y la ventana
// actual. Más lento y más preciso que la correlación de fase; útil como
// refinamiento cuando hay que clavar la posición (p. ej. anillo de totalidad).
class EccDiscDetector final : public IDiscDetector
{
public:
    explicit EccDiscDetector(const DiscTrackerParams& params);

    DiscMethod method() const override { return DiscMethod::Ecc; }

    std::vector<DiscDetection> detect(const DetectorContext& ctx) override;

    // Guarda el parche de referencia alrededor del disco confirmado.
    void onConfirmed(const DetectorContext& ctx,
                     const cv::Point2f& confirmedCenter) override;

    void onMissed() override {}

private:
    DiscTrackerParams p_;
    cv::Mat reference_;      // parche flotante (CV_32F, suavizado)
};
