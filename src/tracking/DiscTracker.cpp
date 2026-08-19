#include "tracking/DiscTracker.h"

#include "tracking/CircleEstimator.h"

#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

DiscTracker::DiscTracker(const DiscTrackerParams& params)
    : p_(params)
    , motion_(std::max(1, p_.lostAfterMisses))
{
}

void DiscTracker::init(const cv::Point2f& center, float radius)
{
    radius_ = radius;
    motion_.reset(center);
}

void DiscTracker::reset()
{
    radius_ = 0.f;
    motion_.reset(cv::Point2f());
}

DiscTrack DiscTracker::track(const cv::Mat& bgr)
{
    DiscTrack out;
    if (radius_ <= 0.f || bgr.empty())
        return out;

    cv::Mat gray;
    cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, cv::Size(0, 0), 1.2);

    const auto sample = [&gray](const cv::Point2f& pt) -> uchar {
        const int x = cvRound(pt.x);
        const int y = cvRound(pt.y);
        if (x < 0 || y < 0 || x >= gray.cols || y >= gray.rows)
            return 0;
        return gray.at<uchar>(y, x);
    };

    // Centro predicho por el modelo de movimiento (avanza sin medición).
    const cv::Point2f pred = motion_.update(false, motion_.position());

    // Recoge puntos del limbo: a lo largo de cada rayo, el punto donde la
    // intensidad cae (brillante→oscuro) con mayor contraste.
    std::vector<cv::Point2f> limbs;
    limbs.reserve(static_cast<size_t>(p_.rays));

    const float rStart = radius_ * (1.f - p_.bandScale);
    const float rEnd = radius_ * (1.f + p_.bandScale);
    const int rMin = std::max(1, static_cast<int>(std::ceil(rStart)));
    const int rMax = std::max(rMin, static_cast<int>(std::floor(rEnd)));

    for (int i = 0; i < p_.rays; ++i) {
        const double angle = 2.0 * CV_PI * static_cast<double>(i) / p_.rays;
        const cv::Point2f dir(static_cast<float>(std::cos(angle)),
                              static_cast<float>(std::sin(angle)));

        cv::Point2f bestPoint;
        float bestScore = 0.f;
        for (int r = rMin; r <= rMax; ++r) {
            const cv::Point2f mid = pred + dir * static_cast<float>(r);
            if (mid.x < 4 || mid.y < 4 || mid.x >= gray.cols - 4 ||
                mid.y >= gray.rows - 4)
                break;
            const float score = static_cast<float>(sample(mid - dir * 3.f)) -
                                static_cast<float>(sample(mid + dir * 3.f));
            if (score > bestScore) {
                bestScore = score;
                bestPoint = mid;
            }
        }
        if (bestScore >= p_.contrastThreshold)
            limbs.push_back(bestPoint);
    }

    bool found = false;
    cv::Point2f measured = pred;
    bool strongFit = false;
    if (!limbs.empty()) {
        const CircleEstimate est = CircleEstimator::fit(limbs, pred, radius_,
                                                        p_.radiusTolerance);
        if (est.ok) {
            found = est.inlierRatio >= p_.acceptRatio;
            measured = est.center;
            strongFit = est.inlierRatio >= p_.validRatio;
        }
    }

    const cv::Point2f center = motion_.update(found, measured);

    out.center = center;
    out.radius = radius_;
    if (found) {
        out.status = strongFit ? TrackStatus::VALID : TrackStatus::UNCERTAIN;
        out.predicted = false;
    } else {
        out.status = motion_.status();
        out.predicted = true;
    }
    return out;
}