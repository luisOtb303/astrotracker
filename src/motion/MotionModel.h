#pragma once

#include "motion/Kalman.h"
#include "motion/TrackStatus.h"

#include <opencv2/core.hpp>

// Modelo de movimiento del objeto: combina las mediciones del tracker con la
// predicción Kalman y gestiona el estado del seguimiento. Cuando el tracker no
// confirma (nube, oclusión), el modelo sigue prediciendo y degrada el estado a
// UNCERTAIN/LOST, pero nunca "desaparece" la posición estimada.
class MotionModel
{
public:
    explicit MotionModel(int lostAfterMisses = 3);

    void reset(const cv::Point2f& pos);

    // Procesa una medición (found = el tracker confirmó) y devuelve la posición
    // estimada actual del objeto.
    cv::Point2f update(bool found, const cv::Point2f& measuredPos);

    TrackStatus status() const { return status_; }
    cv::Point2f position() const { return pos_; }
    int consecutiveMisses() const { return misses_; }

private:
    Kalman kf_;
    int lostAfterMisses_;
    int misses_ = 0;
    TrackStatus status_ = TrackStatus::LOST;
    cv::Point2f pos_;
};