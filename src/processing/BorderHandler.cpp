#include "processing/BorderHandler.h"

#include <opencv2/imgproc.hpp>

cv::Mat BorderHandler::apply(const cv::Mat& frame, const cv::Point2f& offset, BorderMode mode)
{
    const int borderMode = (mode == BorderMode::Replicate)
                               ? cv::BORDER_REPLICATE
                               : cv::BORDER_CONSTANT;

    cv::Mat out;
    const cv::Mat M = (cv::Mat_<double>(2, 3) << 1.0, 0.0, offset.x,
                       0.0, 1.0, offset.y);
    cv::warpAffine(frame, out, M, frame.size(),
                   cv::INTER_LINEAR, borderMode);
    return out;
}