#include "tracking/TemplateDiscDetector.h"

#include <opencv2/imgproc.hpp>

TemplateDiscDetector::TemplateDiscDetector(const DiscTrackerParams& params)
    : p_(params)
{
}

std::vector<DiscDetection> TemplateDiscDetector::detect(const DetectorContext& ctx)
{
    std::vector<DiscDetection> out;
    if (!ctx.norm || template_.empty() || ctx.radius <= 0.f)
        return out;

    // Plantilla en la ventana de búsqueda (si cabe).
    const cv::Rect& win = ctx.searchWindow;
    if (win.width > 8 && win.height > 8 && template_.cols < win.width &&
        template_.rows < win.height) {
        cv::Mat res;
        cv::matchTemplate((*ctx.norm)(win), template_, res, cv::TM_SQDIFF_NORMED);
        double minVal = 0.0;
        cv::Point minLoc;
        cv::minMaxLoc(res, &minVal, nullptr, &minLoc, nullptr);
        if (minVal <= p_.templateSqMax) {
            DiscDetection d;
            d.found = true;
            d.center = cv::Point2f(win.x + minLoc.x + template_.cols * 0.5f,
                                   win.y + minLoc.y + template_.rows * 0.5f);
            d.radius = ctx.radius;
            d.confidence = 0.9f;
            d.method = DiscMethod::Template;
            d.spanDeg = 360.f;
            out.push_back(d);
        }
    }

    // Plantilla en todo el frame (salto grande).
    if (template_.cols < ctx.norm->cols && template_.rows < ctx.norm->rows) {
        cv::Mat res;
        cv::matchTemplate(*ctx.norm, template_, res, cv::TM_SQDIFF_NORMED);
        double minVal = 0.0;
        cv::Point minLoc;
        cv::minMaxLoc(res, &minVal, nullptr, &minLoc, nullptr);
        if (minVal <= p_.templateSqMax) {
            DiscDetection d;
            d.found = true;
            d.center = cv::Point2f(minLoc.x + template_.cols * 0.5f,
                                   minLoc.y + template_.rows * 0.5f);
            d.radius = ctx.radius;
            d.confidence = 0.7f;
            d.method = DiscMethod::Template;
            d.spanDeg = 360.f;
            out.push_back(d);
        }
    }
    return out;
}

void TemplateDiscDetector::onConfirmed(const DetectorContext& ctx,
                                       const cv::Point2f& confirmedCenter)
{
    if (!ctx.norm || ctx.radius <= 0.f)
        return;
    const int half = std::max(8, cvRound(ctx.radius * 1.1f));
    cv::Rect rect(cvRound(confirmedCenter.x) - half,
                  cvRound(confirmedCenter.y) - half, 2 * half, 2 * half);
    rect &= cv::Rect(0, 0, ctx.norm->cols, ctx.norm->rows);
    if (rect.width <= 8 || rect.height <= 8)
        return;
    template_ = (*ctx.norm)(rect).clone();
}
