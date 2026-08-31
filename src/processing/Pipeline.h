#pragma once

#include "processing/BorderHandler.h"
#include "motion/TrackStatus.h"
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

// Callback de cancelación: si devuelve true, el análisis/exportación se
// detiene cuanto antes (para el botón "Detener").
using PipelineCancel = std::function<bool()>;

// Estadísticas de la pasada de análisis.
struct PipelineStats
{
    int64_t frames = 0;
    int64_t valid = 0;
    int64_t uncertain = 0;
    int64_t lost = 0;
    double meanConfidence = 0.0;
};

// Posición del objeto en un frame durante la pasada de análisis. Se devuelve
// junto a los offsets para que la UI pueda dibujar sobre el visor qué es lo
// que se está siguiendo (qué hace visible el "centrado").
struct TrackSample
{
    cv::Point2f center{0.f, 0.f};
    float radius = 0.f;
    TrackStatus status = TrackStatus::LOST;
    bool predicted = false;
};

// Pipeline de dos pasadas: (1) analizar/seguir/calcular desplazamientos,
// (2) aplicar y codificar. Nunca descarta frames: si el tracking falla, se usa
// la predicción del MotionModel.
class Pipeline
{
public:
    // Pasada 1: sigue el objeto en la ROI y devuelve el desplazamiento de cada
    // frame (en el orden de lectura, comenzando en startUs). Los frames previos
    // a startUs no se estabilizan (offset nulo en la pasada 2). Si samples no
    // es nulo, se rellena con la posición del objeto por frame (mismo índice
    // que offsets) para poder dibujar el seguimiento en la UI.
    static std::vector<cv::Point2f> analyze(const std::string& inPath,
                                            const cv::Rect2f& roi,
                                            const PipelineSettings& settings = PipelineSettings(),
                                            PipelineStats* stats = nullptr,
                                            std::vector<TrackSample>* samples = nullptr,
                                            const PipelineProgress& progress = PipelineProgress(),
                                            int64_t startUs = 0,
                                            const PipelineCancel& cancel = PipelineCancel());

    // Pasada 1 + 2: estabiliza inPath y escribe el resultado en outPath.
    bool run(const std::string& inPath, const std::string& outPath,
             const cv::Rect2f& roi,
             const PipelineSettings& settings = PipelineSettings(),
             PipelineStats* stats = nullptr,
             const PipelineProgress& progress = PipelineProgress(),
             int64_t startUs = 0,
             const PipelineCancel& cancel = PipelineCancel()) const;
};