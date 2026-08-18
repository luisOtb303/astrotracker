#include "stabilization/Stabilizer.h"

#include <opencv2/imgproc.hpp>

Stabilizer::Stabilizer()
    : smoother_(0.3f)
{
}

void Stabilizer::reset(const cv::Size& frameSize, const cv::Point2f& objectCenter)
{
    target_ = TargetPosition::center(frameSize);
    smoother_.reset(objectCenter);
    offset_ = target_.point - objectCenter;
}

cv::Point2f Stabilizer::update(const cv::Point2f& objectCenter)
{
    const cv::Point2f smoothed = smoother_.push(objectCenter);
    offset_ = target_.point - smoothed;
    return offset_;
}

cv::Mat Stabilizer::apply(const cv::Mat& frame, const cv::Point2f& offset)
{
    cv::Mat out;
    const cv::Mat M = (cv::Mat_<double>(2, 3) << 1.0, 0.0, offset.x,
                       0.0, 1.0, offset.y);
    cv::warpAffine(frame, out, M, frame.size());
    return out;
}