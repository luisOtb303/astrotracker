#include "tracking/DiscFusion.h"

DiscFusion::DiscFusion(const LimbScorer& scorer)
    : scorer_(scorer)
{
}

DiscFusionResult DiscFusion::selectByLimbSupport(
    const cv::Mat& gray, float radius, const cv::Point2f& prediction,
    const std::vector<DiscDetection>& candidates) const
{
    DiscFusionResult r;
    r.anchor = prediction;
    r.bestScan = scorer_.scan(gray, prediction, radius);
    for (const DiscDetection& d : candidates) {
        const LimbScan s = scorer_.scan(gray, d.center, radius);
        if (s.rays > r.bestScan.rays ||
            (s.rays == r.bestScan.rays && s.score > r.bestScan.score)) {
            r.bestScan = s;
            r.best = &d;
            r.anchor = d.center;
        }
    }
    return r;
}
