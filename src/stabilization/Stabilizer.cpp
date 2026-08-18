#include "stabilization/Stabilizer.h"

#include "processing/BorderHandler.h"

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
    return BorderHandler::apply(frame, offset, BorderMode::Black);
}