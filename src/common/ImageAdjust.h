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
constexpr int kMinHotSensitivity = 0;
constexpr int kMaxHotSensitivity = 100;
constexpr int kDefaultHotSensitivity = 50;

// Umbral del filtro de píxeles calientes, en sigmas del ruido robusto de la
// toma. El rango NO es arbitrario: está medido sobre los RAW de referencia
// (docs/hotpixel-filter.md). El conteo de detecciones meseta en ~155 a partir de
// k=15, mientras que por debajo el falso positivo se dispara (k=8 -> 34 FP/Mpx,
// k=3 -> 1540 FP/Mpx, que pinta la foto) y por encima de 20 no se gana nada
// (k=30 -> 147). Moverse fuera de [10, 30] no da más agresividad útil, solo
// ruido o una imagen intacta, así que el deslizador se recorta a esa banda
// aunque su recorrido sea 0..100 como el del resto de controles.
constexpr double kHotSigmaMin = 10.0;
constexpr double kHotSigmaMax = 30.0;

// Traduce el valor del deslizador (0..100) a multiplicador de sigma.
// El defecto (50) cae en k=20, la meseta segura.
constexpr double hotSensitivityToSigma(int v)
{
    const double t = v < 0 ? 0.0 : (v > 100 ? 100.0 : static_cast<double>(v)) / 100.0;
    return kHotSigmaMin + t * (kHotSigmaMax - kHotSigmaMin);
}

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
    // Píxeles calientes del sensor (CMOS quemado, típico en Sony): puntos
    // aislados mucho más brillantes que sus vecinos. Desactivado por defecto
    // porque corregirlos de más esVisible (manchas) y corregirlos de menos es
    // inocuo.
    //
    // Para fotos RAW este check NO actúa aquí: el filtro se aplica sobre el
    // mosaico, antes del debayer, dentro de RawDecoder::decode(), que es la
    // única capa donde el defecto conserva su amplitud. Para vídeo, donde el
    // decodificador ya entrega la imagen demosaicada, actúa el mapa temporal
    // que se pasa como `hotMask` a apply().
    bool hotPixels = false;
    // Umbral del filtro, 0..100. Más alto = hace falta más desviación sobre el
    // fondo local para corregir un píxel. Se traduce a k sigmas robustas con
    // hotSensitivityToSigma().
    int hotSensitivity = ImageAdjustLimits::kDefaultHotSensitivity;

    bool operator==(const ImageAdjust& o) const
    {
        return wbWarmth == o.wbWarmth && lpSodium == o.lpSodium &&
               lpMercury == o.lpMercury && exposureEv == o.exposureEv &&
               brightness == o.brightness && contrast == o.contrast &&
               denoise == o.denoise && hotPixels == o.hotPixels &&
               hotSensitivity == o.hotSensitivity;
    }

    bool operator!=(const ImageAdjust& o) const { return !(*this == o); }

    bool isDefault() const
    {
        return wbWarmth == 0 && lpSodium == 0 && lpMercury == 0 &&
               exposureEv == 0 && brightness == 0 && contrast == 0 &&
               denoise == 0 && !hotPixels;
    }
};

namespace img {

// Aplica ajustes de imagen en orden: hot pixels → denoise → LP → WB →
// exposición → contraste/brillo → clamp. Soporta CV_8UC3 y CV_16UC3.
//
// `hotMask` es el mapa de píxeles calientes confirmado fotograma a fotograma
// (HotPixelMap), y es el ÚNICO camino de píxeles calientes por aquí: el filtrado
// espacial de una imagen suelta no se aplica (ver el comentario del paso 1). En
// fotos RAW el filtro va antes del debayer, en RawDecoder::decode().
cv::Mat apply(const cv::Mat& bgr, const ImageAdjust& adj, const cv::Mat& hotMask = {});

// Detecta píxeles calientes en una imagen ya demosaicada: devuelve una máscara
// CV_8U con 255 donde el píxel es un defecto del sensor.
//
// Se apoya en la mediana 3x3 como referencia del entorno: un fotosito quemado
// satura por encima de sus vecinos una y otra vez, mientras que una estrella sin
// muestrear es un punto aislado pero con estructura alrededor, que el
// aislamiento por vecindario descarta.
//
// OJO: en una imagen aislada no hay forma de distinguir un píxel caliente de
// una estrella de un solo píxel, y sobre una imagen demosaicada el criterio es
// además débil (ver docs/hotpixel-filter.md §3.3). Por eso NO se usa como
// corrección automática: solo comodescriptor por fotograma para construir el
// mapa temporal, que sí es fiable porque exige repetición en la misma posición.
//
// `sensitivity` 0..100 = umbral en sigmas del ruido local. La firma devuelve
// la máscara; apply() la usa para corregir sustituyendo por la mediana local.
cv::Mat detectHotPixels(const cv::Mat& frame, int sensitivity);

// Sustituye los píxeles marcados por la mediana de sus vecinos.
cv::Mat correctHotPixels(const cv::Mat& frame, const cv::Mat& mask);

} // namespace img