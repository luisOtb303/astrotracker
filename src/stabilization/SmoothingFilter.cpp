#include "stabilization/SmoothingFilter.h"

#include <algorithm>

SmoothingFilter::SmoothingFilter(float alpha)
    : alpha_(alpha)
{
    setAlpha(alpha_);
}

void SmoothingFilter::setAlpha(float alpha)
{
    alpha_ = std::clamp(alpha, 0.01f, 1.0f);
}

void SmoothingFilter::reset(const cv::Point2f& pos)
{
    value_ = pos;
    initialized_ = true;
}

cv::Point2f SmoothingFilter::push(const cv::Point2f& pos)
{
    if (!initialized_) {
        value_ = pos;
        initialized_ = true;
        return value_;
    }
    value_.x = alpha_ * pos.x + (1.0f - alpha_) * value_.x;
    value_.y = alpha_ * pos.y + (1.0f - alpha_) * value_.y;
    return value_;
}