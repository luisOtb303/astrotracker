#pragma once

#include <opencv2/core.hpp>

// Círculo en píxeles de la imagen de trabajo (equivalente a Rect2f pero para
// el seguimiento del disco Sol/Luna): centro + radio.
struct CircleF {
    cv::Point2f center{0.f, 0.f};
    float radius = 0.f;
};