#include "tracking/DiscArcFit.h"

#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <random>
#include <vector>

namespace {

struct Blob {
    std::vector<cv::Point2f> contour;
    int count = 0;
    int area = 0;
    int width = 0;
    int height = 0;
    cv::Point2f centroid{0.f, 0.f};
};

// Otsu + mayor componente conexo y su contorno exterior (con offset de región).
bool largestBrightBlob(const cv::Mat& gray, const cv::Rect& region, Blob& out)
{
    const cv::Rect clamped = region & cv::Rect(0, 0, gray.cols, gray.rows);
    if (clamped.width <= 16 || clamped.height <= 16)
        return false;

    cv::Mat bin;
    cv::threshold(gray(clamped), bin, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    cv::Mat labels, stats, centroids;
    const int n = cv::connectedComponentsWithStats(bin, labels, stats, centroids);
    if (n < 2)
        return false;

    int bestIdx = -1;
    int bestArea = 0;
    for (int i = 1; i < n; ++i) {
        const int area = stats.at<int>(i, cv::CC_STAT_AREA);
        if (area > bestArea) {
            bestArea = area;
            bestIdx = i;
        }
    }
    if (bestIdx < 0)
        return false;

    cv::Mat mask = (labels == bestIdx);
    std::vector<std::vector<cv::Point>> polys;
    cv::findContours(mask, polys, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    if (polys.empty())
        return false;

    const auto* poly = &polys.front();
    for (const auto& p : polys) {
        if (p.size() > poly->size())
            poly = &p;
    }
    out.count = static_cast<int>(poly->size());
    if (out.count < 5)
        return false;

    out.contour.reserve(static_cast<size_t>(out.count));
    for (const cv::Point& p : *poly)
        out.contour.emplace_back(static_cast<float>(p.x + clamped.x),
                                 static_cast<float>(p.y + clamped.y));
    out.area = stats.at<int>(bestIdx, cv::CC_STAT_AREA);
    out.width = stats.at<int>(bestIdx, cv::CC_STAT_WIDTH);
    out.height = stats.at<int>(bestIdx, cv::CC_STAT_HEIGHT);
    out.centroid = cv::Point2f(static_cast<float>(centroids.at<double>(bestIdx, 0) + clamped.x),
                               static_cast<float>(centroids.at<double>(bestIdx, 1) + clamped.y));
    return true;
}

// Cobertura angular (grados) que ocupan los puntos alrededor de un centro.
float spanDegOf(const std::vector<cv::Point2f>& inliers, const cv::Point2f& c)
{
    if (inliers.size() < 2)
        return 0.f;
    std::vector<float> ang;
    ang.reserve(inliers.size());
    for (const cv::Point2f& p : inliers)
        ang.push_back(std::fmod(std::atan2(p.y - c.y, p.x - c.x) * 180.0 / CV_PI + 360.0,
                               360.0));
    std::sort(ang.begin(), ang.end());
    float maxGap = 0.f;
    for (size_t i = 1; i < ang.size(); ++i)
        maxGap = std::max(maxGap, ang[i] - ang[i - 1]);
    maxGap = std::max(maxGap, ang.front() + 360.f - ang.back());
    return std::max(0.f, 360.f - maxGap);
}

struct CountResult {
    std::vector<cv::Point2f> inliers;
    int support = 0;
};

int countSupport(const std::vector<cv::Point2f>& contour, const cv::Point2f& c,
                 float radius, float tol)
{
    int support = 0;
    const float t2 = tol * tol;
    for (const cv::Point2f& p : contour) {
        const float d2 = (p.x - c.x) * (p.x - c.x) + (p.y - c.y) * (p.y - c.y);
        const float dr = std::fabs(std::sqrt(d2) - radius);
        if (dr <= tol && d2 > t2) // descarta el propio contorno degenerado
            ++support;
    }
    return support;
}

} // namespace

DiscArcEstimate DiscArcFit::fitRadius(const cv::Mat& gray, const cv::Rect& region,
                                      float radiusGuess, float tol)
{
    DiscArcEstimate res;
    Blob blob;
    if (!largestBrightBlob(gray, region, blob))
        return res;

    const std::vector<cv::Point2f>& c = blob.contour;
    const int m = blob.count;
    // Rango plausible del radio según la caja del blob: circuncírculos
    // degenerados (centro lejano, radio gigante) engañan al conteo de soporte
    // porque casi todo el contorno queda "a la misma distancia". Se excluyen.
    const float maxDim = std::max(blob.width, blob.height);
    const float minR = std::max(3.f, 0.20f * maxDim);
    const float maxR = 1.5f * maxDim;

    std::mt19937 rng(12345u);
    float bestR = 0.f;
    std::vector<cv::Point2f> bestInliers;
    for (int it = 0; it < 1200; ++it) {
        std::uniform_int_distribution<int> pick(0, m - 1);
        const int a = pick(rng), b = pick(rng), cc = pick(rng);
        if (a == b || a == cc || b == cc)
            continue;
        const cv::Point2f& p1 = c[a];
        const cv::Point2f& p2 = c[b];
        const cv::Point2f& p3 = c[cc];
        const float ax = p1.x, ay = p1.y, bx = p2.x, by = p2.y, cx = p3.x, cy = p3.y;
        const float d = 2.f * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
        if (std::fabs(d) < 1e-3f)
            continue;
        const float ux = ((ax * ax + ay * ay) * (by - cy) + (bx * bx + by * by) * (cy - ay) +
                          (cx * cx + cy * cy) * (ay - by)) /
                         d;
        const float uy = ((ax * ax + ay * ay) * (cx - bx) + (bx * bx + by * by) * (ax - cx) +
                          (cx * cx + cy * cy) * (bx - ax)) /
                         d;
        const float r = std::sqrt((ax - ux) * (ax - ux) + (ay - uy) * (ay - uy));
        if (!std::isfinite(r) || r < minR || r > maxR)
            continue;
        if (ux < -region.width * 2.f || uy < -region.height * 2.f ||
            ux > gray.cols + region.width * 2.f || uy > gray.rows + region.height * 2.f)
            continue;

        const int support = countSupport(c, cv::Point2f(ux, uy), r, tol);
        if (support > static_cast<int>(bestInliers.size())) {
            bestR = r;
            bestInliers.clear();
            for (const cv::Point2f& p : c) {
                const float dr = std::fabs(std::sqrt((p.x - ux) * (p.x - ux) +
                                                     (p.y - uy) * (p.y - uy)) -
                                           r);
                if (dr <= tol && (p.x - ux) * (p.x - ux) + (p.y - uy) * (p.y - uy) > tol * tol)
                    bestInliers.push_back(p);
            }
        }
    }

    if (bestInliers.size() < 12)
        return res;
    res.ok = true;
    res.radius = bestR;
    res.support = static_cast<int>(bestInliers.size());
    res.contourCount = m;
    res.spanDeg = spanDegOf(bestInliers, res.center);

    // Refinado: centro como media de los inliers (el RANSAC ya dio el radio).
    float cx = 0.f, cy = 0.f;
    for (const cv::Point2f& p : bestInliers) {
        cx += p.x;
        cy += p.y;
    }
    cx /= static_cast<float>(bestInliers.size());
    cy /= static_cast<float>(bestInliers.size());
    res.center = cv::Point2f(cx, cy);
    // Si hay suficiente arco, re-estimar el radio con los inliers.
    if (bestInliers.size() >= 20) {
        float r = 0.f;
        for (const cv::Point2f& p : bestInliers)
            r += std::sqrt((p.x - cx) * (p.x - cx) + (p.y - cy) * (p.y - cy));
        res.radius = r / static_cast<float>(bestInliers.size());
    }
    return res;
}

DiscArcEstimate DiscArcFit::fitDisc(const cv::Mat& gray, const cv::Rect& region,
                                    const cv::Point2f& prior, float radiusGuess, float tol)
{
    DiscArcEstimate best;
    for (float r = radiusGuess * 0.55f; r <= radiusGuess * 1.5f; r += radiusGuess * 0.05f) {
        DiscArcEstimate e = fitFixedRadius(gray, region, prior, r, tol);
        if (!e.ok)
            continue;
        // El disco de verdad recorre un arco ancho; los arcos cortos de halo o
        // pegote quedan descartados salvo que no haya otra cosa.
        if (e.spanDeg >= 90.f &&
            (e.support > best.support || (e.support == best.support && e.spanDeg > best.spanDeg)))
            best = e;
    }
    if (!best.ok) {
        // Sin candidato de arco ancho: el que más sostén tenga.
        for (float r = radiusGuess * 0.55f; r <= radiusGuess * 1.5f; r += radiusGuess * 0.05f) {
            DiscArcEstimate e = fitFixedRadius(gray, region, prior, r, tol);
            if (e.ok && e.support > best.support)
                best = e;
        }
    }
    return best;
}

DiscArcEstimate DiscArcFit::fitFixedRadius(const cv::Mat& gray, const cv::Rect& region,
                                           const cv::Point2f& prior, float radius, float tol)
{
    DiscArcEstimate res;
    Blob blob;
    if (!largestBrightBlob(gray, region, blob) || radius <= 0.f)
        return res;

    const std::vector<cv::Point2f>& c = blob.contour;
    const int m = blob.count;
    const float maxDist = std::max(region.width, region.height) * 1.5f;

    std::mt19937 rng(98765u);
    std::vector<cv::Point2f> bestInliers;
    cv::Point2f bestCenter(0.f, 0.f);
    constexpr int kBest = 900;
    for (int it = 0; it < kBest; ++it) {
        std::uniform_int_distribution<int> pick(0, m - 1);
        const int a = pick(rng), b = pick(rng);
        if (a == b)
            continue;
        const cv::Point2f& p1 = c[a];
        const cv::Point2f& p2 = c[b];
        const float dx = p2.x - p1.x, dy = p2.y - p1.y;
        const float d = std::sqrt(dx * dx + dy * dy);
        if (d < 1e-3f || d > 2.f * radius + 2.f * tol)
            continue;
        const float midx = (p1.x + p2.x) * 0.5f, midy = (p1.y + p2.y) * 0.5f;
        const float h = std::sqrt(std::max(0.f, radius * radius - (d * d) * 0.25f));
        const float ux = -dy / d, uy = dx / d;
        for (int s = -1; s <= 1; s += 2) {
            const cv::Point2f cand(midx + static_cast<float>(s) * ux * h,
                                   midy + static_cast<float>(s) * uy * h);
            if (cv::norm(cand - prior) > maxDist)
                continue;
            const int support = countSupport(c, cand, radius, tol);
            if (support > static_cast<int>(bestInliers.size())) {
                bestCenter = cand;
                bestInliers.clear();
                for (const cv::Point2f& p : c) {
                    const float dr = std::fabs(std::sqrt((p.x - cand.x) * (p.x - cand.x) +
                                                         (p.y - cand.y) * (p.y - cand.y)) -
                                               radius);
                    if (dr <= tol && (p.x - cand.x) * (p.x - cand.x) +
                                             (p.y - cand.y) * (p.y - cand.y) >
                                         tol * tol)
                        bestInliers.push_back(p);
                }
            }
        }
    }

    if (bestInliers.size() < 12)
        return res;
    res.ok = true;
    res.center = bestCenter;
    res.radius = radius;
    res.support = static_cast<int>(bestInliers.size());
    res.contourCount = m;
    res.spanDeg = spanDegOf(bestInliers, bestCenter);
    return res;
}

bool DiscArcFit::blobInfo(const cv::Mat& gray, const cv::Rect& region, cv::Point2f& centroid,
                          int& area, int& width, int& height)
{
    Blob blob;
    if (!largestBrightBlob(gray, region, blob))
        return false;
    centroid = blob.centroid;
    area = blob.area;
    width = blob.width;
    height = blob.height;
    return true;
}