#pragma once

#include "motion/MotionModel.h"
#include "motion/TrackStatus.h"
#include "tracking/DiscDetection.h"
#include "tracking/DiscFusion.h"
#include "tracking/DiscTrackerParams.h"
#include "tracking/IDiscDetector.h"
#include "tracking/LimbScorer.h"
#include "tracking/TrackingProfile.h"

#include <memory>
#include <opencv2/core.hpp>
#include <vector>

class ArcBlobDiscDetector;
class TemplateDiscDetector;

// Resultado del seguimiento del disco en una foto.
struct DiscTrack {
    cv::Point2f center{0.f, 0.f};
    float radius = 0.f;
    TrackStatus status = TrackStatus::LOST;
    // true cuando la posición viene de la predicción (objeto oculto o sin
    // confirmación): el círculo se muestra como "supuesto".
    bool predicted = false;
    // true cuando esta foto volvió a confirmar el disco tras una racha de fotos
    // "supuestas" (objeto oculto o salto grande); predictedBefore indica
    // cuántas fotos supuestas le precedieron.
    bool reacquired = false;
    int predictedBefore = 0;
    // Confianza de la medición (0..1; 0 = solo predicción) y método con el
    // que se obtuvo la posición final.
    float confidence = 0.f;
    DiscMethod method = DiscMethod::Prediction;
};

// Sigue el centro de un disco de radio fijo foto a foto. Orquesta varias
// fuentes de candidatos (detectores: plantilla del último disco confirmado,
// arco visible/blob brillante; más fuentes en el futuro) y elige la medición
// por la fuerza del limbo radial (LimbScorer + DiscFusion); los puntos del
// limbo se ajustan a un círculo de radio fijo (CircleEstimator). Si no hay
// confirmación (nube, montaña, eclipse), se mantiene la posición predicha por
// el modelo de movimiento y el estado se degrada a UNCERTAIN/LOST, ampliando
// progresivamente la ventana de búsqueda. El radio fijado por el usuario
// nunca se modifica.
// Sigue el centro de un disco de radio fijo foto a foto. Orquesta varias
// fuentes de candidatos (detectores: plantilla del último disco confirmado,
// arco visible/blob brillante; más fuentes en el futuro) y elige la medición
// por la fuerza del limbo radial (LimbScorer + DiscFusion); los puntos del
// limbo se ajustan a un círculo de radio fijo (CircleEstimator). Si no hay
// confirmación (nube, montaña, eclipse), se mantiene la posición predicha por
// el modelo de movimiento y el estado se degrada a UNCERTAIN/LOST, ampliando
// progresivamente la ventana de búsqueda. El radio fijado por el usuario
// nunca se modifica.
class DiscTracker
{
public:
    explicit DiscTracker(const DiscTrackerParams& params = {});

    void init(const cv::Point2f& center, float radius);
    void reset();

    bool isInitialized() const { return radius_ > 0.f; }

    // Perfil de objeto: cadena de prioridad de detectores. El perfil AUTO
    // reproduce el comportamiento histórico del motor.
    void setProfile(const TrackingProfile& profile);
    // Override por foto: usar SOLO este método como fuente de candidatos.
    // Prediction restaura el perfil. Los métodos aún sin detector se ignoran.
    void setSingleMethod(DiscMethod method);

    DiscTrack track(const cv::Mat& bgr);

private:
    float searchMargin() const;
    void rebuildDetectors();
    bool hasTemplate() const;

    DiscTrackerParams p_;
    MotionModel motion_;
    float radius_ = 0.f;
    TrackingProfile profile_;
    DiscMethod singleMethod_ = DiscMethod::Prediction;
    LimbScorer scorer_;
    DiscFusion fusion_;
    std::vector<std::unique_ptr<IDiscDetector>> detectors_;
    int searchMisses_ = 0;
    int predictedRun_ = 0;
    bool lastPredicted_ = false;
};
