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
    // true cuando esta foto volvió a confirmar el disco tras una racha de fotos
    // "supuestas" (objeto oculto o salto grande); predictedBefore indica
    // cuántas fotos supuestas le precedieron.
    bool reacquired = false;
    int predictedBefore = 0;
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
    // Ventana de búsqueda base para re-adquirir el disco: R*searchMarginScale,
    // con un mínimo de searchMarginMinPx píxeles. Si el objeto salta entre
    // fotos más de lo que abarca esta ventana, crece searchGrowthPerMiss por
    // cada foto fallida hasta maxSearchFactor*base.
    float searchMarginScale = 0.6f;
    float searchMarginMinPx = 20.f;
    float searchGrowthPerMiss = 0.8f;
    float maxSearchFactor = 8.f;
    // Distancia máxima aceptada de la plantilla al buscarla con matchTemplate
    // (TM_SQDIFF_NORMED; 0 = idéntico). Las regiones planas dan valores altos,
    // así que el mínimo del disco real queda discriminado.
    float templateSqMax = 0.4f;
};

// Sigue el centro de un disco de radio fijo foto a foto. En cada foto busca el
// limbo (paso brillante→oscuro) en una banda radial alrededor del centro
// predicho por el modelo de movimiento; los puntos del limbo se ajustan a un
// círculo de radio fijo (CircleEstimator). Si no hay confirmación (nube,
// montaña, eclipse), se mantiene la posición predicha y el estado se degrada a
// UNCERTAIN/LOST. Para no "atascarse" en la semilla cuando el objeto salta
// fuera de esa banda (deriva típica sin star tracker), la localización gruesa
// usa como plantilla el parche del último disco confirmado (matchTemplate) o,
// en su defecto, el blob brillante más grande, y amplía la ventana de búsqueda
// progresivamente hasta volver a confirmar. El radio fijado por el usuario
// nunca se modifica.
class DiscTracker
{
public:
    explicit DiscTracker(const DiscTrackerParams& params = {});

    void init(const cv::Point2f& center, float radius);
    void reset();

    bool isInitialized() const { return radius_ > 0.f; }

    DiscTrack track(const cv::Mat& bgr);

private:
    float searchMargin() const;
    // Localización gruesa del disco dentro de la ventana de búsqueda centrada
    // en `pred`. Devuelve la calidad del candidato en `coarse`:
    //   0 = nada, 1 = plantilla en ventana, 2 = blob en ventana,
    //   3 = plantilla en todo el frame, 4 = blob en todo el frame.
    int locateCoarse(const cv::Mat& gray, const cv::Point2f& pred,
                     cv::Point2f& coarse) const;
    // El blob brillante con área más parecida a πR² dentro de la región
    // `region` (Otsu + mayor componente conexo). Devuelve su centroide.
    bool locateBlob(const cv::Mat& gray, const cv::Rect& region,
                    cv::Point2f& center) const;
    void refreshTemplate(const cv::Mat& gray, const cv::Point2f& center);

    DiscTrackerParams p_;
    MotionModel motion_;
    float radius_ = 0.f;
    cv::Mat template_;
    int searchMisses_ = 0;
    int predictedRun_ = 0;
    bool lastPredicted_ = false;
};