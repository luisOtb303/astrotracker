#pragma once

#include <opencv2/core.hpp>

namespace wb {

// Ajuste de balance de blancos relativo al original.
// warmth: -100..+100, 0 = sin cambio (como disparó la cámara).
// Positivo = más cálido (rojo sube, azul baja).
cv::Mat apply(const cv::Mat& bgr, int warmth);

} // namespace wb
