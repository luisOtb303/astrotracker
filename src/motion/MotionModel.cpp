#include "motion/MotionModel.h"

#include <algorithm>

MotionModel::MotionModel(int lostAfterMisses)
    : lostAfterMisses_(std::max(1, lostAfterMisses))
{
}

void MotionModel::reset(const cv::Point2f& pos)
{
    kf_.reset(pos.x, pos.y);
    misses_ = 0;
    status_ = TrackStatus::VALID;
    pos_ = pos;
}

cv::Point2f MotionModel::update(bool found, const cv::Point2f& measuredPos)
{
    if (found) {
        misses_ = 0;
        status_ = TrackStatus::VALID;
        kf_.correct(measuredPos.x, measuredPos.y);
    } else {
        ++misses_;
        status_ = (misses_ >= lostAfterMisses_) ? TrackStatus::LOST : TrackStatus::UNCERTAIN;
        kf_.predict();
    }
    pos_.x = kf_.x();
    pos_.y = kf_.y();
    return pos_;
}