#include "tracking/CentroidDiscDetector.h"

#include "tracking/DiscArcFit.h"

CentroidDiscDetector::CentroidDiscDetector(const DiscTrackerParams&)
{
}

void CentroidDiscDetector::onConfirmed(const DetectorContext&,
                                       const cv::Point2f&)
{
}

void CentroidDiscDetector::onMissed() {}

std::vector<DiscDetection> CentroidDiscDetector::detect(const DetectorContext& ctx)
{
    std::vector<DiscDetection> out;
    if (!ctx.gray || ctx.radius <= 0.f)
        return out;

    cv::Point2f centroid;
    int area = 0, bw = 0, bh = 0;
    if (!DiscArcFit::blobInfo(*ctx.gray, ctx.searchWindow, centroid, area, bw, bh))
        return out;

    // Exigencias de disco lleno: redondeado y de área comparable a πR². Sin
    // ellas el centroide no representa el centro del disco.
    if (bw < 8 || bh < 8)
        return out;
    const float aspect = static_cast<float>(bw) / static_cast<float>(bh);
    if (aspect < 0.7f || aspect > 1.4f)
        return out;
    const float discArea = static_cast<float>(CV_PI) * ctx.radius * ctx.radius;
    const float ratio = static_cast<float>(area) / discArea;
    if (ratio < 0.6f || ratio > 1.8f)
        return out;

    DiscDetection d;
    d.found = true;
    d.center = centroid;
    d.radius = ctx.radius;
    // Confianza por parecido del área con la del disco ideal (máx en ratio=1).
    d.confidence = std::max(0.2f, 1.0f - std::fabs(ratio - 1.0f) * 0.5f);
    d.method = DiscMethod::Centroid;
    d.spanDeg = 360.f;
    d.symmetric = true;
    out.push_back(d);
    return out;
}
