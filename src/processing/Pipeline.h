#pragma once

#include "processing/BorderHandler.h"
#include "tracking/TrackingProfile.h"

#include <opencv2/core.hpp>
#include <functional>
#include <string>
#include <vector>

// Tipo de tracker usado por el pipeline. Disc = motor unificado del modo
// Fotos (DiscTracker con perfiles y detectores); funciona sobre el frame
// completo y permite ROI opcional como semilla.
enum class TrackerType
{
    Template,
    Centroid,
    Disc
};

// Parámetros ajustables del pipeline (editables en la UI antes de procesar).
struct PipelineSettings
{
    TrackerType tracker = TrackerType::Template;
    ObjectProfile profile = ObjectProfile::Auto; // solo con TrackerType::Disc
    float searchFactor = 2.5f;
    float smoothingAlpha = 0.3f;
    BorderMode borderMode = BorderMode::Black;
    cv::Point2f target{-1.f, -1.f}; // <0 → centro del frame
};

// Callback de progreso (done/total), invocado desde el hilo de trabajo.
using PipelineProgress = std::function<void(int done, int total)>;

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
    // frame (en el orden de lectura, comenzando en startUs). Los frames previos
    // a startUs no se estabilizan (offset nulo en la pasada 2).
    static std::vector<cv::Point2f> analyze(const std::string& inPath,
                                            const cv::Rect2f& roi,
                                            const PipelineSettings& settings = PipelineSettings(),
                                            PipelineStats* stats = nullptr,
                                            const PipelineProgress& progress = PipelineProgress(),
                                            int64_t startUs = 0);

    // Pasada 1 + 2: estabiliza inPath y escribe el resultado en outPath.
    bool run(const std::string& inPath, const std::string& outPath,
             const cv::Rect2f& roi,
             const PipelineSettings& settings = PipelineSettings(),
             PipelineStats* stats = nullptr,
             const PipelineProgress& progress = PipelineProgress(),
             int64_t startUs = 0) const;
};