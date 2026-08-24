#pragma once

#include <opencv2/core.hpp>

// Métodos de localización del disco. Cada detector declara el suyo; el
// resultado guardado por foto indica con qué método se obtuvo.
enum class DiscMethod {
    Prediction,       // predicción del modelo de movimiento (sin medición)
    Template,         // plantilla del último disco confirmado (matchTemplate)
    ArcBlob,          // arco visible / blob brillante (DiscArcFit)
    KnownRadius,      // círculo de radio conocido sobre umbral (eclipse)
    PhaseCorrelation, // correlación de fase en ROI
    Ecc,              // findTransformECC (traslación/euclídeo fino)
    Features,         // Shi-Tomasi + flujo LK + consenso (textura lunar)
    Centroid          // centroide del disco umbralizado (planetas)
};

// Resultado de un detector: candidato de centro + confianza y soporte.
struct DiscDetection {
    bool found = false;
    cv::Point2f center{0.f, 0.f};
    float radius = 0.f;
    float confidence = 0.f; // 0..1
    DiscMethod method = DiscMethod::Prediction;
    // Soporte del candidato: arco de limbo (grados) y si viene de un blob
    // simétrico (disco lleno/corona: el centroide ES el centro del disco).
    float spanDeg = 0.f;
    bool symmetric = false;
};

// Nombre corto del método para logs e interfaz.
inline const char* methodName(DiscMethod m)
{
    switch (m) {
    case DiscMethod::Template: return "plantilla";
    case DiscMethod::ArcBlob: return "arco";
    case DiscMethod::KnownRadius: return "radio conocido";
    case DiscMethod::PhaseCorrelation: return "correlación de fase";
    case DiscMethod::Ecc: return "ecc";
    case DiscMethod::Features: return "features";
    case DiscMethod::Centroid: return "centroide";
    case DiscMethod::Prediction: break;
    }
    return "predicción";
}
