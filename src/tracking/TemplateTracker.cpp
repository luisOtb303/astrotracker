#include "tracking/TemplateTracker.h"

#include <opencv2/imgproc.hpp>

TemplateTracker::TemplateTracker(float searchFactor, float minConfidence)
    : searchFactor_(searchFactor)
    , minConfidence_(minConfidence)
{
}

bool TemplateTracker::init(const cv::Mat& frame, const cv::Rect2f& roi)
{
    if (frame.empty() || roi.width < 4.f || roi.height < 4.f)
        return false;

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    cv::Rect r = roi;
    r &= cv::Rect(0, 0, gray.cols, gray.rows);
    if (r.width < 4 || r.height < 4)
        return false;

    tmpl_ = gray(r).clone();
    lastRect_ = r;
    initialized_ = true;
    return true;
}

TrackResult TemplateTracker::track(const cv::Mat& frame)
{
    TrackResult res;
    if (!initialized_ || frame.empty())
        return res;

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    const int sw = static_cast<int>(lastRect_.width * searchFactor_);
    const int sh = static_cast<int>(lastRect_.height * searchFactor_);
    const int cx = lastRect_.x + lastRect_.width / 2;
    const int cy = lastRect_.y + lastRect_.height / 2;

    cv::Rect search(cx - sw / 2, cy - sh / 2, sw, sh);
    search &= cv::Rect(0, 0, gray.cols, gray.rows);

    if (search.width < tmpl_.cols || search.height < tmpl_.rows) {
        res.rect = lastRect_;
        return res;
    }

    cv::Mat result;
    cv::matchTemplate(gray(search), tmpl_, result, cv::TM_CCOEFF_NORMED);
    double maxVal = 0.0;
    cv::Point maxLoc;
    cv::minMaxLoc(result, nullptr, &maxVal, nullptr, &maxLoc);

    const cv::Rect2f match(search.x + maxLoc.x, search.y + maxLoc.y,
                           tmpl_.cols, tmpl_.rows);
    lastRect_ = match;

    res.rect = match;
    res.confidence = static_cast<float>(maxVal);
    res.found = res.confidence >= minConfidence_;
    return res;
}