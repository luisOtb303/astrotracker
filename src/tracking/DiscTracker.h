#pragma once

#include "motion/MotionModel.h"
#include "motion/TrackStatus.h"

#include <opencv2/core.hpp>

// Resultado del seguimiento del disco en una foto.
struct DiscTrack {
    cv::Point2f center{0.f, 0.f};
    float radius = 0.f;
    TrackStatus status = TrackStatus::LOST;
    // true cuando la posición viene de la predicción (objeto oculto o sin
    // confirmación): el círculo se muestra como "supuesto".
    bool predicted = false;
};

// Parámetros del seguimiento del disco (Sol/Luna) por perfil radial.
struct DiscTrackerParams {
    int rays = 72;                 // número de rayos desde el centro predicho
    float radiusTolerance = 4.f;   // píxeles de tolerancia del limbo alrededor de R
    float bandScale = 0.25f;       // banda radial de búsqueda: R*(1±bandScale)
    float contrastThreshold = 10.f; // salto de intensidad mínimo del limbo (8-bit)
    float validRatio = 0.40f;      // inliers/rayos para considerar VALID
    float acceptRatio = 0.15f;     // mínimo para aceptar la medición (UNCERTAIN)
    int lostAfterMisses = 3;
};

// Sigue el centro de un disco de radio fijo foto a foto. En cada foto busca el
// limbo (paso brillante→oscuro) en una banda radial alrededor del centro
// predicho por el modelo de movimiento; los puntos del limbo se ajustan a un
// círculo de radio fijo (CircleEstimator). Si no hay confirmación (nube,
// montaña, eclipse), se mantiene la posición predicha y el estado se degrada a
// UNCERTAIN/LOST. El radio fijado por el usuario nunca se modifica.
class DiscTracker
{
public:
    explicit DiscTracker(const DiscTrackerParams& params = {});

    void init(const cv::Point2f& center, float radius);
    void reset();

    bool isInitialized() const { return radius_ > 0.f; }

    DiscTrack track(const cv::Mat& bgr);

private:
    DiscTrackerParams p_;
    MotionModel motion_;
    float radius_ = 0.f;
};