#pragma once

#include "tracking/DiscTrackerParams.h"
#include "tracking/IDiscDetector.h"

#include <opencv2/core.hpp>

// Detector por correlación de fase (FFT): mide el desplazamiento global entre
// el parche de referencia del último disco confirmado y la ventana actual
// alrededor de la predicción. No necesita entender el objeto: funciona con el
// fondo y el disco juntos, lo que lo hace el mejor fallback cuando el limbo
// no se puede confirmar. Subpíxel; rechaza desplazamientos fuera del margen.
class PhaseCorrelationDiscDetector final : public IDiscDetector
{
public:
    explicit PhaseCorrelationDiscDetector(const DiscTrackerParams& params);

    DiscMethod method() const override { return DiscMethod::PhaseCorrelation; }

    std::vector<DiscDetection> detect(const DetectorContext& ctx) override;

    // Guarda el parche de referencia (más grande que la plantilla: contexto
    // alrededor del disco incluido).
    void onConfirmed(const DetectorContext& ctx,
                     const cv::Point2f& confirmedCenter) override;

    void onMissed() override {}

private:
    DiscTrackerParams p_;
    cv::Mat reference_;      // parche flotante con ventana aplicada
    cv::Point2f refCenter_;  // centro del disco en el parche de referencia
};
