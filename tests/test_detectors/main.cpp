#include "tracking/ArcBlobDiscDetector.h"
#include "tracking/CentroidDiscDetector.h"
#include "tracking/DiscDetection.h"
#include "tracking/DiscFusion.h"
#include "tracking/EccDiscDetector.h"
#include "tracking/FeatureDiscDetector.h"
#include "tracking/KnownRadiusDiscDetector.h"
#include "tracking/LimbScorer.h"
#include "tracking/PhaseCorrelationDiscDetector.h"

#include <opencv2/imgproc.hpp>
#include <cstdio>
#include <vector>

// Tests sintéticos de los detectores de disco: KnownRadius, Centroid,
// PhaseCorrelation y la selección por limbo de DiscFusion.

namespace {

int failures = 0;

void expect(bool cond, const char* msg)
{
    if (!cond) {
        std::printf("FAIL: %s\n", msg);
        ++failures;
    }
}

cv::Mat smooth(const cv::Mat& src)
{
    cv::Mat gray;
    if (src.channels() == 1)
        gray = src.clone();
    else
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, cv::Size(0, 0), 1.2);
    return gray;
}

void fillContext(DetectorContext& ctx, const cv::Mat& gray, const cv::Point2f& pred,
                 float radius)
{
    ctx.gray = &gray;
    ctx.norm = nullptr;
    ctx.prediction = pred;
    ctx.radius = radius;
    const int half = 160;
    ctx.searchWindow = cv::Rect(cvRound(pred.x) - half, cvRound(pred.y) - half,
                                2 * half, 2 * half) &
                        cv::Rect(0, 0, gray.cols, gray.rows);
    ctx.allowFullFrame = true;
}

} // namespace

int main()
{
    const float R = 40.f;

    // ---- KnownRadius: creciente parcial (mitad del disco tapada). ----
    {
        cv::Mat img(480, 640, CV_8UC3, cv::Scalar(15, 15, 15));
        const cv::Point2f truth(320, 240);
        cv::circle(img, truth, cvRound(R), cv::Scalar(250, 230, 210), cv::FILLED);
        // Oculta la mitad inferior: queda un arco superior.
        cv::rectangle(img, cv::Rect(0, cvRound(truth.y), 640, 240),
                      cv::Scalar(15, 15, 15), cv::FILLED);

        KnownRadiusDiscDetector det({});
        DetectorContext ctx;
        cv::Mat gray = smooth(img);
        fillContext(ctx, gray, truth + cv::Point2f(8.f, -6.f), R); // predicción imperfecta
        const auto cands = det.detect(ctx);
        bool ok = false;
        for (const auto& d : cands)
            if (d.method == DiscMethod::KnownRadius &&
                cv::norm(d.center - truth) < 4.0)
                ok = true;
        expect(ok && !cands.empty(), "KnownRadius localiza el centro con arco corto");

        // Disco completo: vía área ~πR².
        cv::Mat full(480, 640, CV_8UC3, cv::Scalar(15, 15, 15));
        cv::circle(full, truth, cvRound(R), cv::Scalar(250, 230, 210), cv::FILLED);
        cv::Mat gray2 = smooth(full);
        DetectorContext ctx2;
        fillContext(ctx2, gray2, truth, R);
        const auto cands2 = det.detect(ctx2);
        ok = false;
        for (const auto& d : cands2)
            if (d.symmetric && cv::norm(d.center - truth) < 3.0)
                ok = true;
        expect(ok, "KnownRadius: disco lleno via area/centroide");
    }

    // ---- Centroid: disco lleno; NO creciente. ----
    {
        CentroidDiscDetector det({});
        cv::Mat full(480, 640, CV_8UC3, cv::Scalar(20, 20, 20));
        const cv::Point2f truth(300, 220);
        cv::circle(full, truth, cvRound(R), cv::Scalar(245, 245, 245), cv::FILLED);
        cv::Mat gray = smooth(full);
        DetectorContext ctx;
        fillContext(ctx, gray, truth, R);
        const auto cands = det.detect(ctx);
        bool ok = false;
        for (const auto& d : cands)
            if (d.method == DiscMethod::Centroid &&
                cv::norm(d.center - truth) < 3.0)
                ok = true;
        expect(ok, "Centroid: disco lleno en el centroide");

        // Creciente: debe rechazar (área fuera de rango).
        cv::Mat crescent = full.clone();
        cv::circle(crescent, truth + cv::Point2f(R, 0), cvRound(R * 1.05),
                   cv::Scalar(20, 20, 20), cv::FILLED);
        cv::Mat grayC = smooth(crescent);
        DetectorContext ctxC;
        fillContext(ctxC, grayC, truth, R);
        const auto candsC = det.detect(ctxC);
        bool rejected = true;
        for (const auto& d : candsC)
            if (d.method == DiscMethod::Centroid &&
                cv::norm(d.center - truth) > 6.0)
                rejected = false;
        expect(rejected || candsC.empty(), "Centroid rechaza el creciente");
    }

    // ---- PhaseCorrelation: textura desplazada; verifica convención de signo. ----
    {
        const cv::Point2f center(320, 260);
        const float radius = 50.f;

        // Escena: textura aleatoria fija + disco brillante encima.
        cv::Mat frameA(520, 700, CV_8UC1);
        cv::RNG rng(7);
        cv::randn(frameA, cv::Scalar::all(90), cv::Scalar::all(25));

        // Frame B = misma escena desplazada (+18,-12): el objeto se mueve a la
        // derecha y arriba.
        const cv::Point2f shiftVec(18.f, -12.f);
        cv::Mat frameB(520, 700, CV_8UC1);
        frameB.setTo(cv::Scalar::all(60));
        frameA(cv::Rect(30, 30, 640, 440))
            .copyTo(frameB(cv::Rect(30 + cvRound(shiftVec.x),
                                    30 + cvRound(shiftVec.y), 640, 440)));

        PhaseCorrelationDiscDetector det({});

        // Referencia sobre el frame A centrado en 'center'.
        cv::Mat grayA = smooth(frameA);
        DetectorContext ctxA;
        fillContext(ctxA, grayA, center, radius);
        det.onConfirmed(ctxA, center);

        // Detección sobre el frame B con predicción sin movimiento previo.
        cv::Mat grayB = smooth(frameB);
        DetectorContext ctxB;
        fillContext(ctxB, grayB, center, radius);
        const auto cands = det.detect(ctxB);

        bool ok = false;
        float bestErr = 1e9f;
        for (const auto& d : cands) {
            if (d.method == DiscMethod::PhaseCorrelation) {
                bestErr = static_cast<float>(cv::norm(
                    (d.center - center) - shiftVec));
                if (bestErr < 2.5)
                    ok = true;
            }
        }
        char msg[128];
        std::snprintf(msg, sizeof msg,
                      "PhaseCorrelation recupera el shift (mejor err=%.1f px)",
                      bestErr);
        expect(ok, msg);
    }

    // ---- DiscFusion: gana el candidato con más soporte radial. ----
    {
        LimbScorer scorer({});
        DiscFusion fusion(scorer);
        cv::Mat img(480, 640, CV_8UC3, cv::Scalar(25, 25, 25));
        const cv::Point2f truth(320, 240);
        cv::circle(img, truth, cvRound(R), cv::Scalar(250, 250, 250), cv::FILLED);
        cv::Mat gray = smooth(img);

        std::vector<DiscDetection> candidates;
        DiscDetection good;
        good.found = true;
        good.center = truth + cv::Point2f(3.f, -2.f);
        good.radius = R;
        good.confidence = 0.8f;
        good.method = DiscMethod::ArcBlob;
        candidates.push_back(good);
        DiscDetection outlier;
        outlier.found = true;
        outlier.center = truth + cv::Point2f(180.f, 140.f);
        outlier.radius = R;
        outlier.confidence = 0.9f; // confianza alta pero limbo inexistente ahí
        outlier.method = DiscMethod::Template;
        candidates.push_back(outlier);

        const DiscFusionResult sel =
            fusion.selectByLimbSupport(gray, R, truth, candidates);
        // El outlier tiene confianza alta pero ahí no hay limbo: no puede
        // ganar aunque la predicción (que sí tiene soporte radial) parta
        // como referencia.
        const bool outlierWon =
            sel.best != nullptr &&
            sel.best->method == DiscMethod::Template;
        expect(!outlierWon,
               "DiscFusion: el outlier con confianza alta no gana");
    }

    // ---- ECC: textura desplazada una cantidad pequeña (subpíxel). ----
    {
        const cv::Point2f center(320, 260);
        const float radius = 50.f;
        cv::Mat frameA(520, 700, CV_8UC1);
        cv::RNG rng(11);
        cv::randn(frameA, cv::Scalar::all(90), cv::Scalar::all(25));

        // Frame B = escena desplazada (+7,-5).
        const cv::Point2f shiftVec(7.f, -5.f);
        cv::Mat frameB(520, 700, CV_8UC1);
        frameB.setTo(cv::Scalar::all(60));
        frameA(cv::Rect(30, 30, 640, 440))
            .copyTo(frameB(cv::Rect(30 + cvRound(shiftVec.x),
                                    30 + cvRound(shiftVec.y), 640, 440)));

        EccDiscDetector det({});

        cv::Mat grayA = smooth(frameA);
        DetectorContext ctxA;
        fillContext(ctxA, grayA, center, radius);
        det.onConfirmed(ctxA, center);

        cv::Mat grayB = smooth(frameB);
        DetectorContext ctxB;
        fillContext(ctxB, grayB, center, radius);
        const auto cands = det.detect(ctxB);

        bool ok = false;
        for (const auto& d : cands) {
            if (d.method == DiscMethod::Ecc) {
                const float err = static_cast<float>(
                    cv::norm((d.center - center) - shiftVec));
                if (err < 2.0)
                    ok = true;
            }
        }
        expect(ok, "ECC recupera el shift fino");
    }

    // ---- Features: Shi-Tomasi + LK sobre textura. ----
    {
        const cv::Point2f center(320, 260);
        const float radius = 60.f;
        cv::Mat frameA(520, 700, CV_8UC1);
        cv::RNG rng(23);
        cv::randn(frameA, cv::Scalar::all(100), cv::Scalar::all(30));
        cv::GaussianBlur(frameA, frameA, cv::Size(0, 0), 1.0);

        const cv::Point2f shiftVec(12.f, 8.f);
        cv::Mat frameB(520, 700, CV_8UC1);
        frameB.setTo(cv::Scalar::all(70));
        frameA(cv::Rect(40, 40, 600, 420))
            .copyTo(frameB(cv::Rect(40 + cvRound(shiftVec.x),
                                    40 + cvRound(shiftVec.y), 600, 420)));

        FeatureDiscDetector det({});
        cv::Mat grayB = smooth(frameB);

        // Confirmación en el frame A (captura puntos) y detección en B.
        cv::Mat grayA = smooth(frameA);
        DetectorContext ctxA;
        fillContext(ctxA, grayA, center, radius);
        det.onConfirmed(ctxA, center);

        DetectorContext ctxB;
        fillContext(ctxB, grayB, center, radius);
        const auto cands = det.detect(ctxB);

        bool ok = false;
        for (const auto& d : cands) {
            if (d.method == DiscMethod::Features) {
                const float err = static_cast<float>(
                    cv::norm((d.center - center) - shiftVec));
                if (err < 3.0)
                    ok = true;
            }
        }
        expect(ok, "Features/LK recupera el shift con inliers");
    }

    if (failures == 0)
        std::printf("test_detectors: OK\n");
    else
        std::printf("test_detectors: %d fallos\n", failures);
    return failures == 0 ? 0 : 1;
}
