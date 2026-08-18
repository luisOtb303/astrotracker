#pragma once

#include <opencv2/core.hpp>

// Media móvil exponencial sobre el centro del objeto para suavizar el jitter de
// las mediciones del tracker. alpha en (0, 1]: mayor = respuesta más rápida.
class SmoothingFilter
{
public:
    explicit SmoothingFilter(float alpha = 0.3f);

    void reset(const cv::Point2f& pos);
    cv::Point2f push(const cv::Point2f& pos);
    cv::Point2f value() const { return value_; }
    float alpha() const { return alpha_; }
    void setAlpha(float alpha);

private:
    float alpha_;
    cv::Point2f value_;
    bool initialized_ = false;
};