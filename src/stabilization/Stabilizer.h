#pragma once

#include "stabilization/TargetPosition.h"
#include "stabilization/SmoothingFilter.h"

#include <opencv2/core.hpp>

// Estabilizador por traslación: calcula el desplazamiento (dx, dy) que, aplicado
// al frame, sitúa el centro del objeto en el TargetPosition. El centro del objeto
// se suaviza con SmoothingFilter para reducir el jitter de las mediciones.
class Stabilizer
{
public:
    Stabilizer();

    // Reinicia con el tamaño del frame y el primer centro del objeto.
    void reset(const cv::Size& frameSize, const cv::Point2f& objectCenter);

    // Procesa el centro actual del objeto y devuelve el desplazamiento a aplicar.
    cv::Point2f update(const cv::Point2f& objectCenter);

    // Aplica el desplazamiento indicado a un frame (bordes rellenados en negro).
    static cv::Mat apply(const cv::Mat& frame, const cv::Point2f& offset);

    cv::Point2f offset() const { return offset_; }
    TargetPosition target() const { return target_; }
    void setTarget(const TargetPosition& target) { target_ = target; }

private:
    TargetPosition target_;
    SmoothingFilter smoother_;
    cv::Point2f offset_;
};