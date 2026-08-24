#pragma once

#include "tracking/DiscDetection.h"

#include <opencv2/core.hpp>
#include <vector>

// Contexto de búsqueda de una foto: imagen suavizada (perfil radial), copia
// normalizada (plantilla), predicción del modelo de movimiento y ventana de
// búsqueda alrededor de la predicción.
struct DetectorContext {
    const cv::Mat* gray = nullptr; // gris + GaussianBlur
    const cv::Mat* norm = nullptr; // gris normalizado 0..255
    cv::Point2f prediction{0.f, 0.f};
    float radius = 0.f;            // radio conocido del disco (>0)
    cv::Rect searchWindow;         // ventana alrededor de la predicción
    bool allowFullFrame = false;   // permitir además búsqueda en todo el frame
};

// Fuente de candidatos de centro del disco. Implementaciones: plantilla,
// arco/blob, radio conocido, correlación de fase, ECC, features, centroide.
// Los detectores pueden mantener estado entre fotos (plantilla de referencia,
// frame anterior para flujo óptico) vía onConfirmed/onMissed.
class IDiscDetector
{
public:
    virtual ~IDiscDetector() = default;

    virtual DiscMethod method() const = 0;

    // Devuelve 0..n candidatos para esta foto.
    virtual std::vector<DiscDetection> detect(const DetectorContext& ctx) = 0;

    // El disco fue confirmado en confirmedCenter: actualizar estado interno
    // (plantilla de referencia, parche de fase, puntos de features...).
    virtual void onConfirmed(const DetectorContext& ctx,
                             const cv::Point2f& confirmedCenter) = 0;

    // La foto no tuvo confirmación: crecer ventanas, degradar referencias...
    virtual void onMissed() = 0;
};
