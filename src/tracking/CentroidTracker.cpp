#include "tracking/CentroidTracker.h"

#include <opencv2/imgproc.hpp>
#include <algorithm>

namespace {
cv::Point2f largestComponentCentroid(const cv::Mat& mask)
{
    cv::Mat labels, stats, centroids;
    const int n = cv::connectedComponentsWithStats(mask, labels, stats, centroids, 8, CV_32S);

    int best = -1;
    int bestArea = 0;
    for (int i = 1; i < n; ++i) {
        const int area = stats.at<int>(i, cv::CC_STAT_AREA);
        if (area > bestArea) {
            bestArea = area;
            best = i;
        }
    }
    if (best < 0)
        return {};

    return {static_cast<float>(centroids.at<double>(best, 0)),
            static_cast<float>(centroids.at<double>(best, 1))};
}
} // namespace

CentroidTracker::CentroidTracker(float searchFactor, float minAreaFraction)
    : searchFactor_(searchFactor)
    , minAreaFraction_(minAreaFraction)
{
}

bool CentroidTracker::init(const cv::Mat& frame, const cv::Rect2f& roi)
{
    if (frame.empty() || roi.width < 4.f || roi.height < 4.f)
        return false;

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    cv::Rect r = roi;
    r &= cv::Rect(0, 0, gray.cols, gray.rows);
    if (r.width < 4 || r.height < 4)
        return false;

    lastRect_ = r;
    roi_ = r;
    initialized_ = true;
    return true;
}

TrackResult CentroidTracker::track(const cv::Mat& frame)
{
    TrackResult res;
    if (!initialized_ || frame.empty())
        return res;

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    const int sw = static_cast<int>(roi_.width * searchFactor_);
    const int sh = static_cast<int>(roi_.height * searchFactor_);
    const int cx = lastRect_.x + lastRect_.width / 2;
    const int cy = lastRect_.y + lastRect_.height / 2;

    cv::Rect search(cx - sw / 2, cy - sh / 2, sw, sh);
    search &= cv::Rect(0, 0, gray.cols, gray.rows);
    if (search.empty())
        return res;

    cv::Mat patch = gray(search);
    cv::Mat mask;
    cv::threshold(patch, mask, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    const cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, {3, 3});
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);

    const int area = cv::countNonZero(mask);
    const int minArea = static_cast<int>(roi_.area() * minAreaFraction_);
    if (area < minArea)
        return res;

    const cv::Point2f c = largestComponentCentroid(mask);
    const cv::Point2f center(search.x + c.x, search.y + c.y);

    const cv::Rect2f r(center.x - roi_.width / 2.0f, center.y - roi_.height / 2.0f,
                       roi_.width, roi_.height);
    lastRect_ = r;

    res.rect = r;
    res.confidence = std::min(1.0f, area / static_cast<float>(roi_.area()));
    res.found = true;
    return res;
}