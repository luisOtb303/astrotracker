#pragma once

#include <opencv2/core.hpp>

// Filtro de píxeles calientes sobre el mosaico Bayer, ANTES del debayering.
//
// Por qué aquí y no sobre la imagen demosaicada: un fotosito quemado aporta su
// carga a los 2x2 (o 3x3) píxeles de alrededor, así que al demosaicar la mancha
// se reparte y la mediana local sube con él. Medido sobre los RAW de referencia,
// el exceso del defecto cae de +3059/+6716 DN en el mosaico a +16/+75 DN en la
// imagen demosaicada, y el criterio "soy mucho más brillante que mis vecinos"
// deja de funcionar. Sobre el mosaico el defecto sigue siendo lo que es.
//
// El criterio es robusto y sin parámetros de astronomía:
//
//     z(p) = (p - mediana_local) / max(1.4826 * MAD_local, suelo)
//
// con MAD local = mediana de las desviaciones absolutas de los vecinos del mismo
// color respecto a la mediana. Se marca el fotosito si z > k y, además, si
// ninguno de sus vecinos del mismo color también supera el umbral (aislamiento:
// un defecto es un punto raro, no una región rareada).
//
// Solo se necesita saber qué fotositos comparten color, no qué color es, así que
// el resultado es invariante a la fase del CFA: los cuatro patrones Bayer
// producen el mismo resultado, solo permuta las etiquetas de `phase`.

namespace raw {

// Valores por defecto medidos (docs/hotpixel-filter.md): k en [10, 30] es la
// banda útil. Por debajo de 12 el falso positivo se dispara (k=8 -> 34 FP/Mpx) y
// por encima de 20 no se gana nada (k=30 -> 147 detecciones frente a 155 en 20).
struct HotPixelParams
{
    // Umbral en sigmas robustas. 20 es la meseta segura.
    double k = 20.0;
    // Suelo del sigma: sin él, las zonas planas (MAD = 0) hacen que z diverja.
    double sigmaFloor = 1.0;
    // Radio del vecindario en la sub-imagen de cada color. 2 => 24 vecinos
    // (±4 px en el mosaico). Con menos de ~24 muestras el MAD no es un estimador
    // fiable y el conteo de detecciones no se estabiliza con k.
    int radius = 2;
    // Si es false, solo se cuentan detecciones y no se modifica el mosaico.
    bool correct = true;
};

struct HotPixelResult
{
    int detected = 0;   // fotositos que pasan el criterio
    int corrected = 0;  // fotositos realmente sustituidos por la mediana
    bool bayer = false; // false si el mosaico no describe un Bayer 2x2
};

// `phase[py][px]` es el id de color de la clase de paridad (y&1, x&1), con
// valores 0..3. Sirve únicamente como validación: debe describir un Bayer 2x2,
// es decir contener exactamente TRES ids distintos, porque el verde ocupa dos
// de las cuatro paridades (RGGB -> R,G,G,B). Si no, el mosaico no es un Bayer 2x2
// y la función no toca nada (result.bayer = false).
//
// El ids de color no se usa para nada más: el detector agrupa por paridad, y por
// eso da igual la fase concreta.
HotPixelResult removeHotPixels(cv::Mat& mosaic, const int phase[2][2],
                               const HotPixelParams& params);

} // namespace raw