#include "tracking/CircleEstimator.h"

#include <algorithm>
#include <cmath>

CircleEstimate CircleEstimator::fit(const std::vector<cv::Point2f>& points,
                                    const cv::Point2f& priorCenter, float radius,
                                    float tolerance)
{
    if (points.empty() || radius <= 0.f)
        return {};

    // Iteración de punto fijo que minimiza Σ(||p - c|| - R)² con radio fijo:
    // cada punto se proyecta sobre la circunferencia desde el centro actual y
    // el centro nuevo es la media de las proyecciones.
    auto iterate = [&](const std::vector<cv::Point2f>& pts, cv::Point2f c) {
        for (int iter = 0; iter < 40; ++iter) {
            cv::Point2f sum(0.f, 0.f);
            int n = 0;
            for (const cv::Point2f& p : pts) {
                const float dist = cv::norm(p - c);
                if (dist < 1e-3f)
                    continue;
                sum += p + (c - p) * (radius / dist);
                ++n;
            }
            if (n == 0)
                return c;
            const cv::Point2f cNext = sum / static_cast<float>(n);
            if (cv::norm(cNext - c) < 1e-3f)
                return cNext;
            c = cNext;
        }
        return c;
    };

    cv::Point2f c = iterate(points, priorCenter);

    // Descarta outliers: puntos que no quedan sobre la circunferencia guiada.
    std::vector<cv::Point2f> inliers;
    inliers.reserve(points.size());
    for (const cv::Point2f& p : points) {
        if (std::abs(cv::norm(p - c) - radius) <= tolerance)
            inliers.push_back(p);
    }
    if (inliers.empty())
        return {false, c, radius, 0.f};

    // Reajuste final solo con los inliers.
    c = iterate(inliers, c);

    const float ratio = static_cast<float>(inliers.size()) / static_cast<float>(points.size());
    return {true, c, radius, ratio};
}