#include "tracking/ArcBlobDiscDetector.h"

#include "tracking/DiscArcFit.h"

ArcBlobDiscDetector::ArcBlobDiscDetector(const DiscTrackerParams&)
{
}

bool ArcBlobDiscDetector::isSymmetricBlob(float radius, int area, int width,
                                          int height) const
{
    if (area <= 0 || width < 8 || height < 8 || radius <= 0.f)
        return false;
    const float discArea = static_cast<float>(CV_PI) * radius * radius;
    if (static_cast<float>(area) < 0.35f * discArea ||
        static_cast<float>(area) > 4.5f * discArea)
        return false;
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    return aspect >= 0.7f && aspect <= 1.4f;
}

std::vector<DiscDetection> ArcBlobDiscDetector::detect(const DetectorContext& ctx)
{
    std::vector<DiscDetection> out;
    if (!ctx.gray || ctx.radius <= 0.f)
        return out;

    // En una región: blob simétrico (centroide = centro del disco) y arco de
    // limbo de radio fijo, evitando duplicados a <8 px entre ambos.
    const auto addRegion = [&](const cv::Rect& region, float minSpan,
                               float confidence, bool symmetricAllowed) {
        cv::Point2f centroid;
        int area = 0, bw = 0, bh = 0;
        const bool hasBlob = DiscArcFit::blobInfo(*ctx.gray, region, centroid,
                                                  area, bw, bh);
        if (symmetricAllowed && hasBlob &&
            isSymmetricBlob(ctx.radius, area, bw, bh)) {
            bool dup = false;
            for (const DiscDetection& e : out) {
                if (cv::norm(e.center - centroid) < 8.f)
                    dup = true;
            }
            if (!dup) {
                DiscDetection d;
                d.found = true;
                d.center = centroid;
                d.radius = ctx.radius;
                d.confidence = confidence;
                d.method = DiscMethod::ArcBlob;
                d.spanDeg = 360.f;
                d.symmetric = true;
                out.push_back(d);
            }
        }
        const DiscArcEstimate arc = DiscArcFit::fitFixedRadius(
            *ctx.gray, region, ctx.prediction, ctx.radius, 3.f);
        if (arc.ok && arc.spanDeg >= minSpan) {
            bool dup = false;
            for (const DiscDetection& e : out) {
                if (cv::norm(e.center - arc.center) < 8.f)
                    dup = true;
            }
            if (!dup) {
                DiscDetection d;
                d.found = true;
                d.center = arc.center;
                d.radius = ctx.radius;
                d.confidence = confidence;
                d.method = DiscMethod::ArcBlob;
                d.spanDeg = arc.spanDeg;
                out.push_back(d);
            }
        }
    };

    const cv::Rect& win = ctx.searchWindow;
    const cv::Rect full(0, 0, ctx.gray->cols, ctx.gray->rows);
    if (win.width > 32 && win.height > 32)
        addRegion(win, 40.f, 0.8f, true);
    if (ctx.allowFullFrame)
        addRegion(full, 40.f, 0.6f, true);
    return out;
}

void ArcBlobDiscDetector::onConfirmed(const DetectorContext&,
                                      const cv::Point2f&)
{
}

void ArcBlobDiscDetector::onMissed() {}
