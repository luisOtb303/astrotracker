#pragma once

#include "processing/BorderHandler.h"

#include <opencv2/core.hpp>
#include <string>
#include <vector>

// Estadísticas de la pasada de análisis.
struct PipelineStats
{
    int64_t frames = 0;
    int64_t valid = 0;
    int64_t uncertain = 0;
    int64_t lost = 0;
    double meanConfidence = 0.0;
};

// Pipeline de dos pasadas: (1) analizar/seguir/calcular desplazamientos,
// (2) aplicar y codificar. Nunca descarta frames: si el tracking falla, se usa
// la predicción del MotionModel.
class Pipeline
{
public:
    // Pasada 1: sigue el objeto en la ROI y devuelve el desplazamiento de cada
    // frame (en el orden de lectura, comenzando por el frame 0).
    static std::vector<cv::Point2f> analyze(const std::string& inPath,
                                            const cv::Rect2f& roi,
                                            PipelineStats* stats = nullptr);

    // Pasada 1 + 2: estabiliza inPath y escribe el resultado en outPath.
    bool run(const std::string& inPath, const std::string& outPath,
             const cv::Rect2f& roi,
             BorderMode borderMode = BorderMode::Black,
             PipelineStats* stats = nullptr) const;
};