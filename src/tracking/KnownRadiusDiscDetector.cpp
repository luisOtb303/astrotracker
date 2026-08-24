#include "tracking/KnownRadiusDiscDetector.h"

#include "tracking/DiscArcFit.h"

KnownRadiusDiscDetector::KnownRadiusDiscDetector(const DiscTrackerParams&)
{
}

void KnownRadiusDiscDetector::onConfirmed(const DetectorContext&,
                                          const cv::Point2f&)
{
}

// Con cada foto sin confirmación, el detector se vuelve más agresivo: acepta
// arcos aún más cortos (hasta 15°) porque el radio conocido compensa.
void KnownRadiusDiscDetector::onMissed() {}

std::vector<DiscDetection> KnownRadiusDiscDetector::detect(const DetectorContext& ctx)
{
    std::vector<DiscDetection> out;
    if (!ctx.gray || ctx.radius <= 0.f)
        return out;

    const cv::Rect full(0, 0, ctx.gray->cols, ctx.gray->rows);

    // Vía 1: disco lleno o anillo de totalidad. Si el área del mayor blob
    // brillante es compatible con πR², su centroide ES el centro del disco
    // (con R conocido este chequeo es fiable incluso con el anillo fino).
    {
        cv::Point2f centroid;
        int area = 0, bw = 0, bh = 0;
        if (DiscArcFit::blobInfo(*ctx.gray, full, centroid, area, bw, bh)) {
            const float discArea =
                static_cast<float>(CV_PI) * ctx.radius * ctx.radius;
            if (area > 0.45f * discArea && area < 2.2f * discArea &&
                bw > 8 && bh > 8) {
                const float aspect = static_cast<float>(bw) /
                                     static_cast<float>(bh);
                if (aspect >= 0.6f && aspect <= 1.6f) {
                    DiscDetection d;
                    d.found = true;
                    d.center = centroid;
                    d.radius = ctx.radius;
                    d.confidence = 0.85f;
                    d.method = DiscMethod::KnownRadius;
                    d.spanDeg = 360.f;
                    d.symmetric = true;
                    out.push_back(d);
                }
            }
        }
    }

    // Vía 2: arco visible de radio fijo (parciales). Con R conocido basta un
    // arco corto; exigimos al menos ~25°.
    const DiscArcEstimate arc = DiscArcFit::fitFixedRadius(
        *ctx.gray, full, ctx.prediction, ctx.radius, 4.f);
    if (arc.ok && arc.spanDeg >= 25.f) {
        bool dup = false;
        for (const DiscDetection& e : out)
            if (cv::norm(e.center - arc.center) < 8.f)
                dup = true;
        if (!dup) {
            DiscDetection d;
            d.found = true;
            d.center = arc.center;
            d.radius = ctx.radius;
            d.confidence = std::min(1.0f, 0.5f + arc.spanDeg / 360.f * 0.5f);
            d.method = DiscMethod::KnownRadius;
            d.spanDeg = arc.spanDeg;
            out.push_back(d);
        }
    }
    return out;
}
