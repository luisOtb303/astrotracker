#include "tracking/EccDiscDetector.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/video.hpp>
#include <algorithm>
#include <cmath>

EccDiscDetector::EccDiscDetector(const DiscTrackerParams& params)
    : p_(params)
{
}

// Parche cuadrado flotante centrado en c (lado ~3R, impar) y suavizado.
static bool extractPatch(const cv::Mat& gray, const cv::Point2f& c, float radius,
                         cv::Mat& patchOut)
{
    if (gray.empty() || radius <= 0.f)
        return false;
    const int side = std::max(32, cvRound(radius * 6.0f) | 1);
    const int half = side / 2;
    if (c.x - half < 0 || c.y - half < 0 ||
        c.x + (side - half) > gray.cols || c.y + (side - half) > gray.rows)
        return false;
    cv::Mat patch = gray(cv::Rect(cvRound(c.x) - half, cvRound(c.y) - half,
                                  side, side)).clone();
    cv::GaussianBlur(patch, patchOut, cv::Size(0, 0), 1.0);
    patchOut.convertTo(patchOut, CV_32F);
    return true;
}

std::vector<DiscDetection> EccDiscDetector::detect(const DetectorContext& ctx)
{
    std::vector<DiscDetection> out;
    if (!ctx.gray || ctx.radius <= 0.f || reference_.empty())
        return out;

    const int side = reference_.cols;
    const int half = side / 2;
    if (ctx.prediction.x - half < 0 || ctx.prediction.y - half < 0 ||
        ctx.prediction.x + (side - half) > ctx.gray->cols ||
        ctx.prediction.y + (side - half) > ctx.gray->rows)
        return out;

    cv::Mat curPatch;
    cv::Mat((*ctx.gray)(cv::Rect(cvRound(ctx.prediction.x) - half,
                                 cvRound(ctx.prediction.y) - half, side, side)))
        .clone()
        .copyTo(curPatch);
    cv::GaussianBlur(curPatch, curPatch, cv::Size(0, 0), 1.0);
    curPatch.convertTo(curPatch, CV_32F);

    // Warp inicial: identidad. ECC estima W tal que input(W(x)) ≈ template(x).
    // Para TRANSLATION, W = [1 0 dx; 0 1 dy] con el contenido desplazado +d
    // entre la referencia y el frame actual.
    cv::Mat warp = cv::Mat::eye(2, 3, CV_32F);
    double cc = 0.0;
    try {
        const cv::TermCriteria crit(cv::TermCriteria::COUNT | cv::TermCriteria::EPS,
                                    50, 0.001);
        cc = cv::findTransformECC(reference_, curPatch, warp,
                                  cv::MOTION_TRANSLATION, crit);
    } catch (const cv::Exception&) {
        return out; // divergencia de ECC: sin medición
    }
    if (cc < 0.5) // correlación baja: alineamiento no fiable
        return out;

    const cv::Point2f shiftF(warp.at<float>(0, 2), warp.at<float>(1, 2));
    const float maxShift =
        std::max(p_.searchMarginScale * ctx.radius, p_.searchMarginMinPx) * 0.4f;
    if (cv::norm(shiftF) > maxShift)
        return out;

    DiscDetection d;
    d.found = true;
    d.center = ctx.prediction + shiftF; // ver nota geométrica arriba
    d.radius = ctx.radius;
    d.confidence = static_cast<float>(std::min(1.0, std::max(0.0, cc)));
    d.method = DiscMethod::Ecc;
    d.spanDeg = 360.f;
    out.push_back(d);
    return out;
}

void EccDiscDetector::onConfirmed(const DetectorContext& ctx,
                                  const cv::Point2f& confirmedCenter)
{
    if (!ctx.gray)
        return;
    cv::Mat patch;
    if (extractPatch(*ctx.gray, confirmedCenter, ctx.radius, patch))
        reference_ = patch;
}
