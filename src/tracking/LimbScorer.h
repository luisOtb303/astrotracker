#pragma once

#include "tracking/DiscTrackerParams.h"

#include <opencv2/core.hpp>
#include <vector>

// Puntuación radial del limbo: a lo largo de cada rayo desde un centro, el
// punto donde la intensidad cae (brillante→oscuro) con mayor contraste dentro
// de la banda R*(1±bandScale). El disco real tiene un borde definido; el halo
// y las fases difusas puntúan bajo. Es el criterio común con el que se
// comparan los candidatos de los detectores.
struct LimbScan {
    std::vector<cv::Point2f> limbs;
    float score = 0.f; // suma de contrastes de los rayos con limbo
    int rays = 0;      // nº de rayos con limbo
};

class LimbScorer
{
public:
    explicit LimbScorer(const DiscTrackerParams& params);

    LimbScan scan(const cv::Mat& gray, const cv::Point2f& center,
                  float radius) const;

private:
    DiscTrackerParams p_;
};
