#include "tracking/DiscTracker.h"

#include "tracking/CircleEstimator.h"
#include "tracking/DiscArcFit.h"

#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
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

bool DiscTracker::isSymmetricBlob(int area, int width, int height) const
{
    if (area <= 0 || width < 8 || height < 8 || radius_ <= 0.f)
        return false;
    const float discArea = static_cast<float>(CV_PI) * radius_ * radius_;
    if (static_cast<float>(area) < 0.35f * discArea ||
        static_cast<float>(area) > 4.5f * discArea)
        return false;
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    return aspect >= 0.7f && aspect <= 1.4f;
}

void DiscTracker::findCandidates(const cv::Mat& gray, const cv::Point2f& pred,
                                 std::vector<CoarseHit>& out) const
{
    const int half = std::max(1, cvRound(searchMargin()));
    cv::Rect win(cvRound(pred.x) - half, cvRound(pred.y) - half, 2 * half, 2 * half);
    win &= cv::Rect(0, 0, gray.cols, gray.rows);

    // matchTemplate en ventana (si la plantilla cabe).
    if (!template_.empty() && win.width > 8 && win.height > 8 &&
        template_.cols < win.width && template_.rows < win.height) {
        cv::Mat res;
        cv::matchTemplate(gray(win), template_, res, cv::TM_SQDIFF_NORMED);
        double minVal = 0.0;
        cv::Point minLoc;
        cv::minMaxLoc(res, &minVal, nullptr, &minLoc, nullptr);
        if (minVal <= p_.templateSqMax) {
            CoarseHit h;
            h.quality = 1;
            h.center = cv::Point2f(win.x + minLoc.x + template_.cols * 0.5f,
                                   win.y + minLoc.y + template_.rows * 0.5f);
            h.spanDeg = 360.f;
            out.push_back(h);
        }
    }

    // matchTemplate en todo el frame (salto grande).
    if (!template_.empty() && template_.cols < gray.cols &&
        template_.rows < gray.rows) {
        cv::Mat res;
        cv::matchTemplate(gray, template_, res, cv::TM_SQDIFF_NORMED);
        double minVal = 0.0;
        cv::Point minLoc;
        cv::minMaxLoc(res, &minVal, nullptr, &minLoc, nullptr);
        if (minVal <= p_.templateSqMax) {
            CoarseHit h;
            h.quality = 3;
            h.center = cv::Point2f(minLoc.x + template_.cols * 0.5f,
                                   minLoc.y + template_.rows * 0.5f);
            h.spanDeg = 360.f;
            out.push_back(h);
        }
    }

    // Arco del blob: en la ventana y, si no hay nada, en todo el frame. El arco
    // visible reconstruye el centro del disco en fases parciales/crecientes/corona.
    const auto addArc = [&](const cv::Rect& region, float minSpan, int q) {
        cv::Point2f centroid;
        int area = 0, bw = 0, bh = 0;
        const bool hasBlob = DiscArcFit::blobInfo(gray, region, centroid, area, bw, bh);
        if (hasBlob && isSymmetricBlob(area, bw, bh)) {
            bool dup = false;
            for (const CoarseHit& e : out) {
                if (cv::norm(e.center - centroid) < 8.f)
                    dup = true;
            }
            if (!dup) {
                CoarseHit h;
                h.quality = q;
                h.center = centroid;
                h.spanDeg = 360.f;
                h.symmetric = true;
                out.push_back(h);
            }
        }
        const DiscArcEstimate arc = DiscArcFit::fitFixedRadius(gray, region, pred, radius_, 3.f);
        if (arc.ok && arc.spanDeg >= minSpan) {
            bool dup = false;
            for (const CoarseHit& e : out) {
                if (cv::norm(e.center - arc.center) < 8.f)
                    dup = true;
            }
            if (!dup) {
                CoarseHit h;
                h.quality = q;
                h.center = arc.center;
                h.spanDeg = arc.spanDeg;
                out.push_back(h);
            }
        }
    };

    if (win.width > 32 && win.height > 32)
        addArc(win, 40.f, 2);
    if (out.empty())
        addArc(cv::Rect(0, 0, gray.cols, gray.rows), 40.f, 4);
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
    // Copia normalizada solo para la búsqueda por plantilla (SQDIFF no es
    // invariante a la exposición): reduce las diferencias de tono entre RAW sin
    // alterar el perfil radial (que necesita el contraste real del limbo).
    cv::Mat norm;
    cv::normalize(gray, norm, 0, 255, cv::NORM_MINMAX);

    const auto sample = [&gray](const cv::Point2f& pt) -> uchar {
        const int x = cvRound(pt.x);
        const int y = cvRound(pt.y);
        if (x < 0 || y < 0 || x >= gray.cols || y >= gray.rows)
            return 0;
        return gray.at<uchar>(y, x);
    };

    // Centro predicho por el modelo de movimiento (avanza sin medición).
    const cv::Point2f pred = motion_.update(false, motion_.position());

    // Recoge puntos del limbo: a lo largo de cada rayo, el punto donde la
    // intensidad cae (brillante→oscuro) con mayor contraste. Devuelve también
    // la fuerza del gradiente para comparar candidatos de centro.
    const float rStart = radius_ * (1.f - p_.bandScale);
    const float rEnd = radius_ * (1.f + p_.bandScale);
    const int rMin = std::max(1, static_cast<int>(std::ceil(rStart)));
    const int rMax = std::max(rMin, static_cast<int>(std::floor(rEnd)));

    struct RadialScan {
        std::vector<cv::Point2f> limbs;
        float score = 0.f;
        int rays = 0;
    };
    const auto scan = [&](const cv::Point2f& c) {
        RadialScan s;
        s.limbs.reserve(static_cast<size_t>(p_.rays));
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
            if (bestScore >= p_.contrastThreshold) {
                s.limbs.push_back(bestPoint);
                s.score += bestScore;
                ++s.rays;
            }
        }
        return s;
    };

    // Localización gruesa: se generan candidatos (predict, plantilla, arco del
    // blob, blob simétrico) y se elige el que muestre el limbo radial más
    // fuerte. El disco real tiene un borde definido; el halo y las fases
    // difusas puntúan bajo.
    std::vector<CoarseHit> cands;
    findCandidates(norm, pred, cands);

    const CoarseHit* bestHit = nullptr;
    RadialScan bestScan = scan(pred);
    cv::Point2f anchor = pred;
    for (const CoarseHit& hit : cands) {
        const RadialScan s = scan(hit.center);
        if (s.rays > bestScan.rays ||
            (s.rays == bestScan.rays && s.score > bestScan.score)) {
            bestScan = s;
            bestHit = &hit;
            anchor = hit.center;
        }
    }

    CircleEstimate est = CircleEstimator::fit(bestScan.limbs, anchor, radius_,
                                              p_.radiusTolerance);
    // Segunda pasada de refinado: durante la re-adquisición el primer ajuste
    // puede juntar solo arcos parciales y quedar desplazado hacia la posición
    // gruesa; escanear de nuevo desde el centro medido converge al centro real.
    if (est.ok && est.inlierRatio >= p_.acceptRatio &&
        cv::norm(est.center - anchor) < 3.f * radius_) {
        const CircleEstimate refined =
            CircleEstimator::fit(scan(est.center).limbs, est.center, radius_,
                                 p_.radiusTolerance);
        if (refined.ok && refined.inlierRatio >= p_.acceptRatio)
            est = refined;
    }

    bool found = false;
    cv::Point2f measured = pred;
    bool strongFit = false;
    if (est.ok && est.inlierRatio >= p_.acceptRatio) {
        found = true;
        measured = est.center;
        strongFit = est.inlierRatio >= p_.validRatio;
    }

    // Sin plantilla todavía (primera foto): si el ajuste fino no convence, la
    // semilla pintada por el usuario es la medición inicial. Esto además crea
    // la plantilla con la que re-adquirir el disco en las fotos siguientes.
    if (template_.empty() && !found) {
        found = true;
        measured = pred;
        strongFit = false;
    } else if (!found && bestHit) {
        if (bestHit->quality == 1 || bestHit->quality == 3) {
            // La plantilla localizó el disco con confianza aunque el ajuste fino
            // falle (bajo contraste del limbo): usar su centro.
            found = true;
            measured = bestHit->center;
            strongFit = false;
        } else if (bestHit->symmetric || bestHit->spanDeg >= 50.f) {
            // Arco de limbo fuerte o blob simétrico (disco lleno/corona): el
            // centro del círculo que forma la fase visible es ya el centro del
            // disco, que es lo que importa para centrar la foto.
            found = true;
            measured = bestHit->center;
            strongFit = false;
        }
    }

    const cv::Point2f center = motion_.update(found, measured);

    out.center = center;
    out.radius = radius_;
    if (found) {
        out.status = strongFit ? TrackStatus::VALID : TrackStatus::UNCERTAIN;
        out.predicted = false;
        refreshTemplate(norm, center);
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