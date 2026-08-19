#pragma once

#include <opencv2/core.hpp>
#include <string>

// Decodificador de archivos RAW (CR2, CR3 y demás formatos soportados por
// LibRaw). Decodificación bajo demanda, sin macros globales: cada llamada abre
// su propia instancia, por lo que es seguro usarlo desde hilos de trabajo.
class RawDecoder
{
public:
    // True si LibRaw reconoce el archivo como RAW (solo lee la cabecera).
    static bool isRawFile(const std::string& path);

    // Dimensiones de la imagen decodificada (cabecera, sin decodificar píxeles).
    static bool dimensions(const std::string& path, int& w, int& h);

    // Procesa el RAW y devuelve la imagen en BGR. want16 → CV_16UC3 sin
    // comprimir el rango (para exportación); si no, CV_8UC3 con mapeo de tono.
    // maxDim > 0 activa el medio tamaño del sensor cuando conviene y permite
    // análisis a menor resolución.
    static bool decode(const std::string& path, cv::Mat& out, bool want16 = false,
                       int maxDim = 0);

    // Miniatura embebida en el RAW (JPEG rápido), útil para filmstrips.
    // Devuelve BGR8, ajustada a maxDim si se indica.
    static bool thumbnail(const std::string& path, cv::Mat& out, int maxDim = 0);
};