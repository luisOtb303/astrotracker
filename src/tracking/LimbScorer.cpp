#include "tracking/LimbScorer.h"

#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>

LimbScorer::LimbScorer(const DiscTrackerParams& params)
    : p_(params)
{
}

LimbScan LimbScorer::scan(const cv::Mat& gray, const cv::Point2f& center,
                          float radius) const
{
    LimbScan s;
    if (gray.empty() || radius <= 0.f)
        return s;
    s.limbs.reserve(static_cast<size_t>(p_.rays));

    const auto sample = [&gray](const cv::Point2f& pt) -> uchar {
        const int x = cvRound(pt.x);
        const int y = cvRound(pt.y);
        if (x < 0 || y < 0 || x >= gray.cols || y >= gray.rows)
            return 0;
        return gray.at<uchar>(y, x);
    };

    const float rStart = radius * (1.f - p_.bandScale);
    const float rEnd = radius * (1.f + p_.bandScale);
    const int rMin = std::max(1, static_cast<int>(std::ceil(rStart)));
    const int rMax = std::max(rMin, static_cast<int>(std::floor(rEnd)));

    for (int i = 0; i < p_.rays; ++i) {
        const double angle = 2.0 * CV_PI * static_cast<double>(i) / p_.rays;
        const cv::Point2f dir(static_cast<float>(std::cos(angle)),
                              static_cast<float>(std::sin(angle)));

        cv::Point2f bestPoint;
        float bestScore = 0.f;
        for (int r = rMin; r <= rMax; ++r) {
            const cv::Point2f mid = center + dir * static_cast<float>(r);
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
        if (bestScore >= p_.contrastThreshold) {
            s.limbs.push_back(bestPoint);
            s.score += bestScore;
            ++s.rays;
        }
    }
    return s;
}
