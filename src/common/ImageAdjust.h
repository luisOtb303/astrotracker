#pragma once

#include <opencv2/core.hpp>

namespace ImageAdjustLimits {

constexpr int kMinKelvin = 2500;
constexpr int kMaxKelvin = 20000;
constexpr int kDefaultKelvin = 11250;
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
    int wbKelvin = 0;   // 0 = auto (detectado = identidad); 2500..20000
    int lpSodium = 0;   // 0..100  (589 nm — farolas sodio)
    int lpMercury = 0;  // 0..100  (436/546/578 nm — farolas mercurio)
    int exposureEv = 0; // -40..+40 (x10, 0.0 = sin cambio)
    int brightness = 0; // -100..+100 (offset aditivo)
    int contrast = 0;   // -100..+100 (alrededor del punto medio)
    int denoise = 0;    // 0..100 (fastNlMeans h; 0 = off)

    bool operator==(const ImageAdjust& o) const
    {
        return wbKelvin == o.wbKelvin && lpSodium == o.lpSodium &&
               lpMercury == o.lpMercury && exposureEv == o.exposureEv &&
               brightness == o.brightness && contrast == o.contrast &&
               denoise == o.denoise;
    }

    bool operator!=(const ImageAdjust& o) const { return !(*this == o); }

    bool isDefault() const
    {
        return wbKelvin == 0 && lpSodium == 0 && lpMercury == 0 &&
               exposureEv == 0 && brightness == 0 && contrast == 0 &&
               denoise == 0;
    }
};

namespace img {

// Aplica ajustes de imagen en orden: denoise → LP → WB → exposición →
// contraste/brillo → clamp. Soporta CV_8UC3 y CV_16UC3.
// detectedKelvin: valor EXIF de la foto actual (0 = desconocido, usa 11250).
cv::Mat apply(const cv::Mat& bgr, const ImageAdjust& adj,
              int detectedKelvin = 0);

} // namespace img
