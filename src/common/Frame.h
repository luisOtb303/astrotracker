#pragma once

#include <opencv2/core.hpp>
#include <cstdint>

// Un frame de vídeo decodificado con sus metadatos.
struct Frame
{
    int64_t index = -1; // número de frame (0-based, estimado por PTS)
    int64_t ptsUs = 0;  // timestamp del frame en microsegundos
    cv::Mat image;      // imagen BGR8 (para SER 16-bit se convertirá)
};