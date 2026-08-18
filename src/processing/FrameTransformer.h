#pragma once

#include "processing/BorderHandler.h"

#include <opencv2/core.hpp>

// Aplica la transformación de traslación a un frame respetando el modo de borde.
class FrameTransformer
{
public:
    explicit FrameTransformer(BorderMode mode = BorderMode::Black)
        : mode_(mode)
    {
    }

    cv::Mat transform(const cv::Mat& frame, const cv::Point2f& offset) const
    {
        return BorderHandler::apply(frame, offset, mode_);
    }

    void setBorderMode(BorderMode mode) { mode_ = mode; }

private:
    BorderMode mode_;
};