#include "tracking/DiscTracker.h"

#include "tracking/ArcBlobDiscDetector.h"
#include "tracking/CircleEstimator.h"
#include "tracking/DiscFusion.h"
#include "tracking/LimbScorer.h"
#include "tracking/TemplateDiscDetector.h"

#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>

DiscTracker::DiscTracker(const DiscTrackerParams& params)
    : p_(params)
    , motion_(std::max(1, p_.lostAfterMisses))
    , scorer_(p_)
    , fusion_(scorer_)
    , templateDet_(new TemplateDiscDetector(p_))
    , arcDet_(new ArcBlobDiscDetector(p_))
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

    // Centro predicho por el modelo de movimiento (avanza sin medición).
    const cv::Point2f pred = motion_.update(false, motion_.position());

    // Ventana de búsqueda alrededor de la predicción; crece con los fallos.
    const int half = std::max(1, cvRound(searchMargin()));
    cv::Rect win(cvRound(pred.x) - half, cvRound(pred.y) - half, 2 * half, 2 * half);
    win &= cv::Rect(0, 0, gray.cols, gray.rows);

    DetectorContext ctx;
    ctx.gray = &gray;
    ctx.norm = &norm;
    ctx.prediction = pred;
    ctx.radius = radius_;
    ctx.searchWindow = win;
    ctx.allowFullFrame = false;

    // Localización gruesa: candidatos de los detectores. La búsqueda en todo
    // el frame solo se activa si la ventana no dio nada (comportamiento
    // histórico: la plantilla en ventana bloquea el arco a frame completo).
    std::vector<DiscDetection> cands = templateDet_->detect(ctx);
    ctx.allowFullFrame = cands.empty();
    for (DiscDetection& d : arcDet_->detect(ctx))
        cands.push_back(std::move(d));

    // Selección: gana el candidato con el limbo radial más fuerte; el disco
    // real tiene un borde definido y el halo o las fases difusas puntúan bajo.
    const DiscFusionResult sel =
        fusion_.selectByLimbSupport(gray, radius_, pred, cands);

    CircleEstimate est = CircleEstimator::fit(sel.bestScan.limbs, sel.anchor,
                                              radius_, p_.radiusTolerance);
    // Segunda pasada de refinado: durante la re-adquisición el primer ajuste
    // puede juntar solo arcos parciales y quedar desplazado hacia la posición
    // gruesa; escanear de nuevo desde el centro medido converge al centro real.
    if (est.ok && est.inlierRatio >= p_.acceptRatio &&
        cv::norm(est.center - sel.anchor) < 3.f * radius_) {
        const CircleEstimate refined = CircleEstimator::fit(
            scorer_.scan(gray, est.center, radius_).limbs, est.center, radius_,
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
    if (!templateDet_->hasTemplate() && !found) {
        found = true;
        measured = pred;
        strongFit = false;
    } else if (!found && sel.best) {
        if (sel.best->method == DiscMethod::Template) {
            // La plantilla localizó el disco con confianza aunque el ajuste fino
            // falle (bajo contraste del limbo): usar su centro.
            found = true;
            measured = sel.best->center;
            strongFit = false;
        } else if (sel.best->symmetric || sel.best->spanDeg >= 50.f) {
            // Arco de limbo fuerte o blob simétrico (disco lleno/corona): el
            // centro del círculo que forma la fase visible es ya el centro del
            // disco, que es lo que importa para centrar la foto.
            found = true;
            measured = sel.best->center;
            strongFit = false;
        }
    }

    const cv::Point2f center = motion_.update(found, measured);

    out.center = center;
    out.radius = radius_;
    if (found) {
        out.status = strongFit ? TrackStatus::VALID : TrackStatus::UNCERTAIN;
        out.predicted = false;
        templateDet_->onConfirmed(ctx, center);
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
        templateDet_->onMissed();
        arcDet_->onMissed();
    }
    return out;
}
