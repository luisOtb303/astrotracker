#pragma once

#include <opencv2/core.hpp>

namespace ImageAdjustLimits {

constexpr int kMinWarmth = -100;
constexpr int kMaxWarmth = 100;
constexpr int kMinEv = -40;
constexpr int kMaxEv = 40;
constexpr int kMinBrightness = -100;
constexpr int kMaxBrightness = 100;
constexpr int kMinContrast = -100;
constexpr int kMaxContrast = 100;
constexpr int kMinDenoise = 0;
constexpr int kMaxDenoise = 100;

} // namespace ImageAdjustLimits

struct ImageAdjust
{
    // Balance de blancos RELATIVO. No se guardan Kelvin porque el valor real
    // con el que se tomó la foto no se puede conocer de forma fiable (el EXIF
    // casi nunca lo trae y el decodificador ya aplica su propia corrección):
    // 0 = la imagen tal cual sale del decodificador, + = más cálido, - = más frío.
    int wbWarmth = 0;    // -100..+100
    int lpSodium = 0;    // 0..100  (589 nm — farolas sodio)
    int lpMercury = 0;   // 0..100  (436/546/578 nm — farolas mercurio)
    int exposureEv = 0;  // -40..+40 (x10, 0.0 = sin cambio)
    int brightness = 0;  // -100..+100 (offset aditivo)
    int contrast = 0;    // -100..+100 (alrededor del punto medio)
    int denoise = 0;     // 0..100 (fastNlMeans h; 0 = off)

    bool operator==(const ImageAdjust& o) const
    {
        return wbWarmth == o.wbWarmth && lpSodium == o.lpSodium &&
               lpMercury == o.lpMercury && exposureEv == o.exposureEv &&
               brightness == o.brightness && contrast == o.contrast &&
               denoise == o.denoise;
    }

    bool operator!=(const ImageAdjust& o) const { return !(*this == o); }

    bool isDefault() const
    {
        return wbWarmth == 0 && lpSodium == 0 && lpMercury == 0 &&
               exposureEv == 0 && brightness == 0 && contrast == 0 &&
               denoise == 0;
    }
};

namespace img {

// Aplica ajustes de imagen en orden: denoise → LP → WB → exposición →
// contraste/brillo → clamp. Soporta CV_8UC3 y CV_16UC3.
cv::Mat apply(const cv::Mat& bgr, const ImageAdjust& adj);

} // namespace img