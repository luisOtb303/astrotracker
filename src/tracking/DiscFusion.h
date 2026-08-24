#pragma once

#include "tracking/DiscDetection.h"
#include "tracking/LimbScorer.h"

#include <opencv2/core.hpp>
#include <vector>

// Fusión de candidatos de varios detectores. Etapa actual: selección por
// soporte radial del limbo (comportamiento histórico del motor): se escanea
// la predicción como referencia y gana el candidato con más rayos con limbo
// (desempate por suma de gradientes). El consenso entre fuentes y el rechazo
// de outliers se añaden con los detectores independientes.
struct DiscFusionResult {
    cv::Point2f anchor{0.f, 0.f};
    LimbScan bestScan;
    const DiscDetection* best = nullptr; // nullptr → ganó la predicción
};

class DiscFusion
{
public:
    explicit DiscFusion(const LimbScorer& scorer);

    DiscFusionResult selectByLimbSupport(const cv::Mat& gray, float radius,
                                         const cv::Point2f& prediction,
                                         const std::vector<DiscDetection>& candidates) const;

private:
    const LimbScorer& scorer_;
};
