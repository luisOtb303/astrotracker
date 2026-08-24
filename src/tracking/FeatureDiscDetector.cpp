#include "tracking/FeatureDiscDetector.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/video.hpp>
#include <algorithm>
#include <cmath>

FeatureDiscDetector::FeatureDiscDetector(const DiscTrackerParams& params)
    : p_(params)
{
}

void FeatureDiscDetector::onMissed()
{
    prevGray_.release();
    prevPoints_.clear();
}

std::vector<DiscDetection> FeatureDiscDetector::detect(const DetectorContext& ctx)
{
    std::vector<DiscDetection> out;
    if (!ctx.gray || ctx.radius <= 0.f || prevGray_.empty() || prevPoints_.size() < 8)
        return out;
    if (prevGray_.size() != ctx.gray->size())
        return out;

    // Flujo óptico piramidal de los puntos del frame anterior al actual.
    std::vector<cv::Point2f> nextPts;
    std::vector<uchar> status;
    std::vector<float> err;
    cv::calcOpticalFlowPyrLK(prevGray_, *ctx.gray, prevPoints_, nextPts, status,
                             err, cv::Size(21, 21), 3,
                             cv::TermCriteria(cv::TermCriteria::COUNT |
                                                  cv::TermCriteria::EPS,
                                              30, 0.01),
                             0, 0.001);

    // Flujos válidos y mediana por componente (robusta a outliers).
    std::vector<float> dxs, dys;
    for (size_t i = 0; i < status.size(); ++i) {
        if (!status[i])
            continue;
        dxs.push_back(nextPts[i].x - prevPoints_[i].x);
        dys.push_back(nextPts[i].y - prevPoints_[i].y);
    }
    const int tracked = static_cast<int>(std::min(dxs.size(), dys.size()));
    if (tracked < 8)
        return out;

    const auto median = [](std::vector<float> v) {
        std::sort(v.begin(), v.end());
        return v[v.size() / 2];
    };
    const float mdx = median(dxs);
    const float mdy = median(dys);

    // Inliers: flujos cerca de la mediana → confianza.
    int inliers = 0;
    for (int i = 0; i < tracked; ++i) {
        if (std::hypot(dxs[i] - mdx, dys[i] - mdy) < 2.5f)
            ++inliers;
    }
    const float ratio = static_cast<float>(inliers) / static_cast<float>(tracked);
    if (inliers < 6 || ratio < 0.4f)
        return out;

    DiscDetection d;
    d.found = true;
    d.center = prevCenter_ + cv::Point2f(mdx, mdy);
    d.radius = ctx.radius;
    d.confidence = std::min(1.0f, ratio * 0.9f + 0.1f);
    d.method = DiscMethod::Features;
    d.spanDeg = 360.f;
    out.push_back(d);
    return out;
}

void FeatureDiscDetector::onConfirmed(const DetectorContext& ctx,
                                      const cv::Point2f& confirmedCenter)
{
    if (!ctx.gray)
        return;
    // Puntos Shi-Tomasi dentro de una caja ~3R alrededor del centro.
    const int half = std::max(20, cvRound(ctx.radius * 1.5f));
    cv::Rect box(cvRound(confirmedCenter.x) - half,
                 cvRound(confirmedCenter.y) - half, 2 * half, 2 * half);
    box &= cv::Rect(0, 0, ctx.gray->cols, ctx.gray->rows);
    if (box.width < 24 || box.height < 24)
        return;

    cv::Mat mask = cv::Mat::zeros(ctx.gray->size(), CV_8U);
    mask(box).setTo(255);
    std::vector<cv::Point2f> pts;
    cv::goodFeaturesToTrack(*ctx.gray, pts, 60, 0.01, 4.0, mask, 3);
    prevPoints_ = std::move(pts);
    prevGray_ = ctx.gray->clone();
    prevCenter_ = confirmedCenter;
}
