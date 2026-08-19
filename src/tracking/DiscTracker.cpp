#include "tracking/DiscTracker.h"

#include "tracking/CircleEstimator.h"

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

bool DiscTracker::locateBlob(const cv::Mat& gray, const cv::Rect& region,
                             cv::Point2f& center) const
{
    const cv::Rect clamped = region & cv::Rect(0, 0, gray.cols, gray.rows);
    if (clamped.width <= 8 || clamped.height <= 8)
        return false;

    cv::Mat bin;
    cv::threshold(gray(clamped), bin, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    cv::Mat labels, stats, centroids;
    const int n = cv::connectedComponentsWithStats(bin, labels, stats, centroids);
    if (n < 2)
        return false;

    const float minArea = 0.05f * static_cast<float>(CV_PI) * radius_ * radius_;
    const float maxArea = 1.6f * static_cast<float>(CV_PI) * radius_ * radius_;
    const float target = static_cast<float>(CV_PI) * radius_ * radius_;
    float bestDiff = std::numeric_limits<float>::max();
    int bestIdx = -1;
    for (int i = 1; i < n; ++i) {
        const float area = static_cast<float>(stats.at<int>(i, cv::CC_STAT_AREA));
        if (area >= minArea && area <= maxArea) {
            const float diff = std::fabs(area - target);
            if (diff < bestDiff) {
                bestDiff = diff;
                bestIdx = i;
            }
        }
    }
    if (bestIdx < 0)
        return false;

    center = cv::Point2f(static_cast<float>(centroids.at<double>(bestIdx, 0)),
                         static_cast<float>(centroids.at<double>(bestIdx, 1)));
    center += cv::Point2f(static_cast<float>(clamped.x), static_cast<float>(clamped.y));
    return true;
}

int DiscTracker::locateCoarse(const cv::Mat& gray, const cv::Point2f& pred,
                              cv::Point2f& coarse) const
{
    const int half = std::max(1, cvRound(searchMargin()));
    cv::Rect win(cvRound(pred.x) - half, cvRound(pred.y) - half, 2 * half, 2 * half);
    win &= cv::Rect(0, 0, gray.cols, gray.rows);

    // 1) Plantilla en la ventana: el parche del último disco confirmado. Es el
    //    camino preferido (preciso y discriminante). Se omite en la primera
    //    foto (aún no hay plantilla), donde la medida es la semilla del usuario.
    if (!template_.empty() && win.width > 8 && win.height > 8 &&
        template_.cols < win.width && template_.rows < win.height) {
        cv::Mat res;
        cv::matchTemplate(gray(win), template_, res, cv::TM_SQDIFF_NORMED);
        double minVal = 0.0;
        cv::Point minLoc;
        cv::minMaxLoc(res, &minVal, nullptr, &minLoc, nullptr);
        if (minVal <= p_.templateSqMax) {
            coarse = cv::Point2f(win.x + minLoc.x + template_.cols * 0.5f,
                                 win.y + minLoc.y + template_.rows * 0.5f);
            return 1;
        }
    }

    // 2) Blob brillante en la ventana (re-adquisición con deriva moderada).
    if (!template_.empty() && locateBlob(gray, win, coarse))
        return 2;

    // 3) Plantilla en todo el frame: cubre saltos grandes entre fotos. Es más
    //    discriminante que el blob (el Sol es el patrón más cercano a la
    //    plantilla, aunque haya halo o glare).
    if (!template_.empty() && template_.cols < gray.cols &&
        template_.rows < gray.rows) {
        cv::Mat res;
        cv::matchTemplate(gray, template_, res, cv::TM_SQDIFF_NORMED);
        double minVal = 0.0;
        cv::Point minLoc;
        cv::minMaxLoc(res, &minVal, nullptr, &minLoc, nullptr);
        if (minVal <= p_.templateSqMax) {
            coarse = cv::Point2f(minLoc.x + template_.cols * 0.5f,
                                 minLoc.y + template_.rows * 0.5f);
            return 3;
        }
    }

    // 4) Blob en todo el frame (último recurso, muy ruidoso en escenas reales).
    if (!template_.empty() &&
        locateBlob(gray, cv::Rect(0, 0, gray.cols, gray.rows), coarse))
        return 4;

    return 0;
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

    // Localización gruesa: si el objeto saltó fuera de la banda radial, la
    // plantilla (o el blob) lo sitúa de nuevo de forma aproximada. Se prioriza
    // la plantilla sobre el blob (más discriminante en escenas reales).
    cv::Point2f coarse;
    const int coarseQ = locateCoarse(norm, pred, coarse);
    const cv::Point2f anchor = (coarseQ > 0) ? coarse : pred;

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
    } else if (!found && (coarseQ == 1 || coarseQ == 3)) {
        // La plantilla sí localizó el disco con confianza aunque el ajuste fino
        // falle (bajo contraste del limbo): usar su centro, que es lo que
        // importa para centrar la foto.
        found = true;
        measured = coarse;
        strongFit = false;
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