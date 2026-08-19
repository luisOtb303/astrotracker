#include "tracking/DiscTracker.h"

#include "tracking/CircleEstimator.h"

#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

DiscTracker::DiscTracker(const DiscTrackerParams& params)
    : p_(params)
    , motion_(std::max(1, p_.lostAfterMisses))
{
}

void DiscTracker::init(const cv::Point2f& center, float radius)
{
    radius_ = radius;
    motion_.reset(center);
}

void DiscTracker::reset()
{
    radius_ = 0.f;
    motion_.reset(cv::Point2f());
}

float DiscTracker::searchMargin() const
{
    const float base = std::max(p_.searchMarginScale * radius_, p_.searchMarginMinPx);
    return std::min(base * (1.f + p_.searchGrowthPerMiss * static_cast<float>(searchMisses_)),
                    base * p_.maxSearchFactor);
}

bool DiscTracker::locateCoarse(const cv::Mat& gray, const cv::Point2f& pred,
                               cv::Point2f& coarse) const
{
    const int half = std::max(1, cvRound(searchMargin()));
    cv::Rect win(cvRound(pred.x) - half, cvRound(pred.y) - half, 2 * half, 2 * half);
    win &= cv::Rect(0, 0, gray.cols, gray.rows);
    if (win.width <= 8 || win.height <= 8)
        return false;

    const cv::Mat window = gray(win);

    // 1) Plantilla: el parche del último disco confirmado. Cuando aún no hay
    //    plantilla (justo la foto de la semilla) se omite: no tiene sentido y
    //    evita contaminar la medida precisa del primer frame.
    if (!template_.empty() && template_.cols < win.width && template_.rows < win.height) {
        cv::Mat res;
        cv::matchTemplate(window, template_, res, cv::TM_CCOEFF_NORMED);
        double maxVal = 0.0;
        cv::Point maxLoc;
        cv::minMaxLoc(res, nullptr, &maxVal, nullptr, &maxLoc);
        if (maxVal >= p_.templateCorrMin) {
            coarse = cv::Point2f(win.x + maxLoc.x + template_.cols * 0.5f,
                                 win.y + maxLoc.y + template_.rows * 0.5f);
            return true;
        }
    }

    // 2) Respaldo: el blob brillante (disco) con área más parecida a πR².
    if (!template_.empty()) {
        cv::Mat bin;
        cv::threshold(window, bin, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
        cv::Mat labels, stats, centroids;
        const int n = cv::connectedComponentsWithStats(bin, labels, stats, centroids);
        if (n > 1) {
            const float minArea = 0.05f * static_cast<float>(CV_PI) * radius_ * radius_;
            const float maxArea = 1.6f * static_cast<float>(CV_PI) * radius_ * radius_;
            float bestArea = 0.f;
            int bestIdx = -1;
            for (int i = 1; i < n; ++i) {
                const float area = static_cast<float>(stats.at<int>(i, cv::CC_STAT_AREA));
                if (area >= minArea && area <= maxArea && area > bestArea) {
                    bestArea = area;
                    bestIdx = i;
                }
            }
            if (bestIdx >= 0) {
                coarse = cv::Point2f(static_cast<float>(centroids.at<double>(bestIdx, 0)),
                                     static_cast<float>(centroids.at<double>(bestIdx, 1)));
                coarse += cv::Point2f(static_cast<float>(win.x), static_cast<float>(win.y));
                return true;
            }
        }
    }

    return false;
}

void DiscTracker::refreshTemplate(const cv::Mat& gray, const cv::Point2f& center)
{
    const int half = std::max(8, cvRound(radius_ * 1.1f));
    cv::Rect rect(cvRound(center.x) - half, cvRound(center.y) - half,
                  2 * half, 2 * half);
    rect &= cv::Rect(0, 0, gray.cols, gray.rows);
    if (rect.width <= 8 || rect.height <= 8)
        return;
    template_ = gray(rect).clone();
}

DiscTrack DiscTracker::track(const cv::Mat& bgr)
{
    DiscTrack out;
    if (radius_ <= 0.f || bgr.empty())
        return out;

    cv::Mat gray;
    cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, cv::Size(0, 0), 1.2);

    const auto sample = [&gray](const cv::Point2f& pt) -> uchar {
        const int x = cvRound(pt.x);
        const int y = cvRound(pt.y);
        if (x < 0 || y < 0 || x >= gray.cols || y >= gray.rows)
            return 0;
        return gray.at<uchar>(y, x);
    };

    // Centro predicho por el modelo de movimiento (avanza sin medición).
    const cv::Point2f pred = motion_.update(false, motion_.position());

    // Localización gruesa: si el objeto saltó fuera de la banda radial, buscar
    // la plantilla (o el blob brillante) lo sitúa de nuevo de forma aproximada.
    cv::Point2f coarse;
    const bool coarseOk = locateCoarse(gray, pred, coarse);
    const cv::Point2f anchor = coarseOk ? coarse : pred;

    // Recoge puntos del limbo: a lo largo de cada rayo, el punto donde la
    // intensidad cae (brillante→oscuro) con mayor contraste.
    const float rStart = radius_ * (1.f - p_.bandScale);
    const float rEnd = radius_ * (1.f + p_.bandScale);
    const int rMin = std::max(1, static_cast<int>(std::ceil(rStart)));
    const int rMax = std::max(rMin, static_cast<int>(std::floor(rEnd)));

    const auto scan = [&](const cv::Point2f& c) {
        std::vector<cv::Point2f> limbs;
        limbs.reserve(static_cast<size_t>(p_.rays));
        for (int i = 0; i < p_.rays; ++i) {
            const double angle = 2.0 * CV_PI * static_cast<double>(i) / p_.rays;
            const cv::Point2f dir(static_cast<float>(std::cos(angle)),
                                  static_cast<float>(std::sin(angle)));

            cv::Point2f bestPoint;
            float bestScore = 0.f;
            for (int r = rMin; r <= rMax; ++r) {
                const cv::Point2f mid = c + dir * static_cast<float>(r);
                if (mid.x < 4 || mid.y < 4 || mid.x >= gray.cols - 4 ||
                    mid.y >= gray.rows - 4)
                    break;
                const float score = static_cast<float>(sample(mid - dir * 3.f)) -
                                    static_cast<float>(sample(mid + dir * 3.f));
                if (score > bestScore) {
                    bestScore = score;
                    bestPoint = mid;
                }
            }
            if (bestScore >= p_.contrastThreshold)
                limbs.push_back(bestPoint);
        }
        return limbs;
    };

    CircleEstimate est = CircleEstimator::fit(scan(anchor), anchor, radius_,
                                              p_.radiusTolerance);
    // Segunda pasada de refinado: durante la re-adquisición el primer ajuste
    // puede juntar solo arcos parciales y quedar desplazado hacia la posición
    // gruesa; escanear de nuevo desde el centro medido converge al centro real.
    if (est.ok && est.inlierRatio >= p_.acceptRatio &&
        cv::norm(est.center - anchor) < 3.f * radius_) {
        const CircleEstimate refined =
            CircleEstimator::fit(scan(est.center), est.center, radius_,
                                 p_.radiusTolerance);
        if (refined.ok && refined.inlierRatio >= p_.acceptRatio)
            est = refined;
    }

    bool found = false;
    cv::Point2f measured = pred;
    bool strongFit = false;
    if (est.ok) {
        found = est.inlierRatio >= p_.acceptRatio;
        measured = est.center;
        strongFit = est.inlierRatio >= p_.validRatio;
    }

    const cv::Point2f center = motion_.update(found, measured);

    out.center = center;
    out.radius = radius_;
    if (found) {
        out.status = strongFit ? TrackStatus::VALID : TrackStatus::UNCERTAIN;
        out.predicted = false;
        refreshTemplate(gray, center);
        searchMisses_ = 0;
        if (lastPredicted_) {
            out.reacquired = true;
            out.predictedBefore = predictedRun_;
        }
        lastPredicted_ = false;
        predictedRun_ = 0;
    } else {
        out.status = motion_.status();
        out.predicted = true;
        ++searchMisses_;
        ++predictedRun_;
        lastPredicted_ = true;
    }
    return out;
}