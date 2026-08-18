#pragma once

#include <opencv2/core.hpp>

// Punto objetivo: posición de la imagen donde debe mantenerse el centro del
// objeto. La corrección de cada frame desplaza el objeto hasta este punto.
struct TargetPosition
{
    cv::Point2f point;

    static TargetPosition center(const cv::Size& frameSize)
    {
        return {cv::Point2f(frameSize.width * 0.5f, frameSize.height * 0.5f)};
    }
};