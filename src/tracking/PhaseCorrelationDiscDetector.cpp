#include "tracking/PhaseCorrelationDiscDetector.h"

#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>

PhaseCorrelationDiscDetector::PhaseCorrelationDiscDetector(const DiscTrackerParams& params)
    : p_(params)
{
}

// Extrae un parche cuadrado centrado en c (lado ~2.6R) como float y le aplica
// una ventana de Hanning para que la FFT no vea bordes duros.
static bool extractPatch(const cv::Mat& gray, const cv::Point2f& c, float radius,
                         cv::Mat& patchOut)
{
    if (gray.empty() || radius <= 0.f)
        return false;
    const int side = std::max(32, cvRound(radius * 5.2f) | 1);
    const int half = side / 2;
    // Margen para que el parche quepa entero.
    if (c.x - half < 0 || c.y - half < 0 ||
        c.x + (side - half) > gray.cols || c.y + (side - half) > gray.rows)
        return false;
    cv::Mat patch = gray(cv::Rect(cvRound(c.x) - half, cvRound(c.y) - half,
                                  side, side));
    patch.convertTo(patchOut, CV_32F);
    cv::Mat window(side, side, CV_32F);
    cv::createHanningWindow(window, window.size(), CV_32F);
    patchOut = patchOut.mul(window);
    return true;
}

std::vector<DiscDetection> PhaseCorrelationDiscDetector::detect(const DetectorContext& ctx)
{
    std::vector<DiscDetection> out;
    if (!ctx.gray || ctx.radius <= 0.f || reference_.empty())
        return out;

    const int side = reference_.cols;
    const int half = side / 2;
    // La ventana actual debe contener el parche completo alrededor de la
    // predicción; si el disco está demasiado cerca del borde, no hay medición.
    if (ctx.prediction.x - half < 0 || ctx.prediction.y - half < 0 ||
        ctx.prediction.x + (side - half) > ctx.gray->cols ||
        ctx.prediction.y + (side - half) > ctx.gray->rows)
        return out;

    cv::Mat cur;
    cv::Mat((*ctx.gray)(cv::Rect(cvRound(ctx.prediction.x) - half,
                                 cvRound(ctx.prediction.y) - half, side, side)))
        .convertTo(cur, CV_32F);
    cv::Mat window(side, side, CV_32F);
    cv::createHanningWindow(window, window.size(), CV_32F);
    cur = cur.mul(window);

    double response = 0.0;
    const cv::Point2d shift =
        cv::phaseCorrelate(reference_, cur, cv::noArray(), &response);
    const cv::Point2f shiftF(static_cast<float>(shift.x),
                             static_cast<float>(shift.y));

    // El desplazamiento máximo aceptable: la mitad del margen de búsqueda.
    const float maxShift =
        std::max(p_.searchMarginScale * ctx.radius, p_.searchMarginMinPx) * 0.5f;
    if (cv::norm(shiftF) > maxShift)
        return out;

    // phaseCorrelate(src1=parche referencia, src2=parche actual) devuelve el
    // desplazamiento s tal que src2(x) ≈ src1(x - s): el contenido se movió
    // +s px entre la referencia y ahora. El centro actual es el de referencia
    // más ese desplazamiento (el parche actual está anclado a la predicción).
    DiscDetection d;
    d.found = true;
    d.center = ctx.prediction + shiftF;
    d.radius = ctx.radius;
    // La respuesta de pico (0..1) mide la nitidez de la correlación.
    d.confidence = static_cast<float>(std::min(1.0, response * 4.0));
    d.method = DiscMethod::PhaseCorrelation;
    d.spanDeg = 360.f;
    out.push_back(d);
    return out;
}

void PhaseCorrelationDiscDetector::onConfirmed(const DetectorContext& ctx,
                                               const cv::Point2f& confirmedCenter)
{
    if (!ctx.gray)
        return;
    cv::Mat patch;
    if (extractPatch(*ctx.gray, confirmedCenter, ctx.radius, patch)) {
        reference_ = patch;
        refCenter_ = confirmedCenter;
    }
}
