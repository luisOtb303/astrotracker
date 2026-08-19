#pragma once

#include <opencv2/core.hpp>

// Ajuste de un círculo al ARCO o anillo visible del disco (Sol/Luna) a partir
// del blob brillante de la imagen. En fases parciales (creciente, menguante,
// eclipse parcial) el centro del disco es el círculo que forma el arco visible,
// NO el punto más brillante; esta clase lo reconstruye con RANSAC sobre el
// contorno del blob.
//
// Usos:
//  - Semilla de la app/harness: centro + radio reales del disco (fitRadius).
//  - Referencia de verificación del harness en fases parciales (fitFixedRadius).
//  - Localización gruesa del motor en las mismas fases (fitFixedRadius).
struct DiscArcEstimate {
    bool ok = false;
    cv::Point2f center{0.f, 0.f};
    float radius = 0.f;   // radio fijado o estimado (píxeles)
    int support = 0;      // puntos del contorno que quedan sobre el círculo
    float spanDeg = 0.f;  // cobertura angular del arco sobre el círculo (0..360)
    int contourCount = 0; // puntos totales del contorno analizado
};

class DiscArcFit
{
public:
    // Centro + radio libres del círculo que recorre más puntos del contorno del
    // mayor blob brillante dentro de `region`. Para detectar la semilla.
    static DiscArcEstimate fitRadius(const cv::Mat& gray, const cv::Rect& region,
                                     float radiusGuess, float tol);

    // Barrido de radio fijo alrededor de `radiusGuess`: localiza el centro del
    // disco con el radio real probando varios radios y quedándose con el que
    // consigue el arco más amplio y sostén. Es la forma fiable de sembrar el
    // seguimiento (el ajuste libre sobreajusta a arcos del halo).
    static DiscArcEstimate fitDisc(const cv::Mat& gray, const cv::Rect& region,
                                   const cv::Point2f& prior, float radiusGuess,
                                   float tol);

    // Centro de un círculo de radio fijo `radius` (tolerancia `tol` px) que
    // maximiza los puntos del contorno del mayor blob brillante de `region` a
    // distancia ~radius. Para fases parciales y referencia de verificación.
    static DiscArcEstimate fitFixedRadius(const cv::Mat& gray, const cv::Rect& region,
                                          const cv::Point2f& prior, float radius,
                                          float tol);

    // info básica del mayor blob brillante de `region` (centroide/área/bbox)
    // y si es "simétrico" (disco lleno o corona: centrado y redondeado).
    static bool blobInfo(const cv::Mat& gray, const cv::Rect& region,
                         cv::Point2f& centroid, int& area, int& width, int& height);
};