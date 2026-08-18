#pragma once

#include <opencv2/core.hpp>

// Modo de relleno de los bordes que quedan descubiertos al desplazar el frame.
enum class BorderMode {
    Black,     // bordes negros
    Replicate, // se extiende el valor de los píxeles del borde
};

// Aplica la traslación (dx, dy) a un frame rellenando los bordes según el modo.
class BorderHandler
{
public:
    static cv::Mat apply(const cv::Mat& frame, const cv::Point2f& offset, BorderMode mode);
};