#include "common/WhiteBalance.h"

#include <algorithm>
#include <opencv2/imgproc.hpp>

namespace wb {

cv::Mat apply(const cv::Mat& bgr, int warmth)
{
    if (bgr.empty() || warmth == 0)
        return bgr.clone();

    const float t = static_cast<float>(warmth) / 100.0f; // [-1, +1]
    const float rGain = 1.0f + 0.5f * t;
    const float bGain = 1.0f - 0.5f * t;

    cv::Mat result;
    bgr.convertTo(result, CV_32F);

    std::vector<cv::Mat> ch;
    cv::split(result, ch);
    ch[2] *= rGain; // R
    ch[0] *= bGain; // B
    cv::merge(ch, result);

    cv::Mat out;
    result.convertTo(out, bgr.depth());
    return out;
}

} // namespace wb
